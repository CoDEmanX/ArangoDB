////////////////////////////////////////////////////////////////////////////////
/// @brief utility functions for json objects
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/json-utilities.h"
#include "Basics/fasthash.h"
#include "Basics/hashes.h"
#include "Basics/string-buffer.h"
#include "Basics/Utf8Helper.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

static TRI_json_t* MergeRecursive (TRI_memory_zone_t* zone,
                                   TRI_json_t const* lhs,
                                   TRI_json_t const* rhs,
                                   bool nullMeansRemove,
                                   bool mergeObjects) {
  TRI_json_t* result = TRI_CopyJson(zone, lhs);

  if (result == nullptr) {
    return nullptr;
  }
  
  size_t const n = TRI_LengthVector(&rhs->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    // enumerate all the replacement values
    auto key = static_cast<TRI_json_t*>(TRI_AtVector(&rhs->_value._objects, i));
    auto value = static_cast<TRI_json_t*>(TRI_AtVector(&rhs->_value._objects, i + 1));

    if (value->_type == TRI_JSON_NULL && nullMeansRemove) {
      // replacement value is a null and we don't want to store nulls => delete attribute from the result
      TRI_DeleteObjectJson(zone, result, key->_value._string.data);
    }
    else {
      // replacement value is not a null or we want to store nulls
      TRI_json_t* lhsValue = TRI_LookupObjectJson(lhs, key->_value._string.data);

      if (lhsValue == nullptr) {
        // existing array does not have the attribute => append new attribute
        if (value->_type == TRI_JSON_OBJECT) {
          TRI_json_t* empty = TRI_CreateObjectJson(zone);
          TRI_json_t* merged = MergeRecursive(zone, empty, value, nullMeansRemove, mergeObjects);
          TRI_Insert3ObjectJson(zone, result, key->_value._string.data, merged);

          TRI_FreeJson(zone, empty);
        }
        else {
          TRI_Insert3ObjectJson(zone, result, key->_value._string.data, TRI_CopyJson(zone, value));
        }
      }
      else {
        // existing array already has the attribute => replace attribute
        if (lhsValue->_type == TRI_JSON_OBJECT && value->_type == TRI_JSON_OBJECT && mergeObjects) {
          TRI_json_t* merged = MergeRecursive(zone, lhsValue, value, nullMeansRemove, mergeObjects);
          TRI_ReplaceObjectJson(zone, result, key->_value._string.data, merged);
          TRI_FreeJson(zone, merged);
        }
        else {
          TRI_ReplaceObjectJson(zone, result, key->_value._string.data, value);
        }
      }
    }

  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get type weight of a json value usable for comparison and sorting
////////////////////////////////////////////////////////////////////////////////

static int TypeWeight (TRI_json_t const* value) {
  if (value != nullptr) {
    switch (value->_type) {
      case TRI_JSON_BOOLEAN:
        return 1;
      case TRI_JSON_NUMBER:
        return 2;
      case TRI_JSON_STRING:
      case TRI_JSON_STRING_REFERENCE:
        // a string reference has the same weight as a regular string
        return 3;
      case TRI_JSON_ARRAY:
        return 4;
      case TRI_JSON_OBJECT:
        return 5;
      case TRI_JSON_NULL:
      case TRI_JSON_UNUSED:
        break;
    }
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback function used for json value sorting
////////////////////////////////////////////////////////////////////////////////

static int CompareJson (void const* lhs, 
                        void const* rhs) { 
  return TRI_CompareValuesJson(static_cast<TRI_json_t const*>(lhs), static_cast<TRI_json_t const*>(rhs), true);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two list of array keys, sort them and return a combined list
////////////////////////////////////////////////////////////////////////////////

static TRI_json_t* GetMergedKeyList (TRI_json_t const* lhs,
                                     TRI_json_t const* rhs) {
  TRI_json_t* keys;
  TRI_json_t* unique;
  size_t i, n;

  TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);
  
  n = TRI_LengthVector(&lhs->_value._objects) + TRI_LengthVector(&rhs->_value._objects);

  keys = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, n);

  if (keys == nullptr) {
    return nullptr;
  }

  n = TRI_LengthVector(&lhs->_value._objects);

  for (i = 0 ; i < n; i += 2) {
    auto key = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&lhs->_value._objects, i));

    TRI_ASSERT(TRI_IsStringJson(key));
    TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, keys, key);
  }


  n = TRI_LengthVector(&rhs->_value._objects);

  for (i = 0 ; i < n; i += 2) {
    auto key = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&rhs->_value._objects, i));

    TRI_ASSERT(TRI_IsStringJson(key));
    TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, keys, key);
  }

  // sort the key list in place
  TRI_SortArrayJson(keys);

  // list is now sorted
  unique = TRI_UniquifyArrayJson(keys);

  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);

  return unique; // might be NULL
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief compare two json values
////////////////////////////////////////////////////////////////////////////////

int TRI_CompareValuesJson (TRI_json_t const* lhs,
                           TRI_json_t const* rhs,
                           bool useUTF8) {
  // note: both lhs and rhs may be NULL!
  {
    int lWeight = TypeWeight(lhs);
    int rWeight = TypeWeight(rhs);

    if (lWeight < rWeight) {
      return -1;
    }

    if (lWeight > rWeight) {
      return 1;
    }

    TRI_ASSERT_EXPENSIVE(lWeight == rWeight);
  }

  // lhs and rhs have equal weights
  if (lhs == nullptr) {
    // both lhs and rhs are NULL, so they are equal
    TRI_ASSERT(rhs == nullptr);
    return 0;
  }

  switch (lhs->_type) {
    case TRI_JSON_UNUSED:
    case TRI_JSON_NULL: {
      return 0; // null == null;
    }

    case TRI_JSON_BOOLEAN: {
      if (lhs->_value._boolean == rhs->_value._boolean) {
        return 0;
      }

      if (! lhs->_value._boolean && rhs->_value._boolean) {
        return -1;
      }

      return 1;
    }

    case TRI_JSON_NUMBER: {
      if (lhs->_value._number == rhs->_value._number) {
        return 0;
      }

      if (lhs->_value._number < rhs->_value._number) {
        return -1;
      }

      return 1;
    }

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      // same for STRING and STRING_REFERENCE
      int res;
      if (useUTF8) {
        res = TRI_compare_utf8(lhs->_value._string.data,
                               lhs->_value._string.length - 1, 
                               rhs->_value._string.data,
                               rhs->_value._string.length - 1);
      }
      else {
        res = strcmp(lhs->_value._string.data, rhs->_value._string.data);
      }
      if (res < 0) {
        return -1;
      }
      else if (res > 0) {
        return 1;
      }
      return 0;
    }

    case TRI_JSON_ARRAY: {
      size_t const nl = TRI_LengthVector(&lhs->_value._objects);
      size_t const nr = TRI_LengthVector(&rhs->_value._objects);
      size_t n;

      if (nl > nr) {
        n = nl;
      }
      else {
        n = nr;
      }

      for (size_t i = 0; i < n; ++i) {
        auto lhsValue = (i >= nl) ? nullptr : static_cast<TRI_json_t const*>(TRI_AtVector(&lhs->_value._objects, i));
        auto rhsValue = (i >= nr) ? nullptr : static_cast<TRI_json_t const*>(TRI_AtVector(&rhs->_value._objects, i));
        
        int result = TRI_CompareValuesJson(lhsValue, rhsValue, useUTF8);

        if (result != 0) {
          return result;
        }
      }

      return 0;
    }

    case TRI_JSON_OBJECT: {
      TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
      TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);

      TRI_json_t* keys = GetMergedKeyList(lhs, rhs);

      if (keys != nullptr) {
        size_t const n = TRI_LengthVector(&keys->_value._objects);

        for (size_t i = 0; i < n; ++i) {
          auto keyElement = static_cast<TRI_json_t const*>(TRI_AtVector(&keys->_value._objects, i));
          TRI_ASSERT(TRI_IsStringJson(keyElement));

          TRI_json_t const* lhsValue = TRI_LookupObjectJson(lhs, keyElement->_value._string.data); // may be NULL
          TRI_json_t const* rhsValue = TRI_LookupObjectJson(rhs, keyElement->_value._string.data); // may be NULL

          int result = TRI_CompareValuesJson(lhsValue, rhsValue, useUTF8);

          if (result != 0) {
            TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
            return result;
          }
        }

        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, keys);
      }
      // fall-through to returning 0
    }

  }
  
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief check if two json values are the same
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckSameValueJson (TRI_json_t const* lhs,
                             TRI_json_t const* rhs) {
  return (TRI_CompareValuesJson(lhs, rhs, false) == 0);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a json value is contained in a json list
////////////////////////////////////////////////////////////////////////////////

bool TRI_CheckInArrayJson (TRI_json_t const* search,
                          TRI_json_t const* list) {
  TRI_ASSERT(search);
  TRI_ASSERT(list);
  TRI_ASSERT(list->_type == TRI_JSON_ARRAY);

  // iterate over list
  size_t const n = TRI_LengthVector(&list->_value._objects);

  for (size_t i = 0; i < n; ++i) {
    TRI_json_t const* listValue = static_cast<TRI_json_t const*>(TRI_AtVector(&list->_value._objects, i));

    if (TRI_CheckSameValueJson(search, listValue)) {
      // value is contained in list, exit
      return true;
    }
  }

  // finished list iteration, value not contained
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return the elements of a list that are between the specified bounds
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_BetweenArrayJson (TRI_json_t const* list,
                                 TRI_json_t const* lower,
                                 bool includeLower,
                                 TRI_json_t const* upper,
                                 bool includeUpper) {
  TRI_json_t* result;
  size_t i, n;

  TRI_ASSERT(list);
  TRI_ASSERT(list->_type == TRI_JSON_ARRAY);
  TRI_ASSERT(lower || upper);

  // create result list
  result = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (result == nullptr) {
    return nullptr;
  }

  n = TRI_LengthVector(&list->_value._objects);

  for (i = 0; i < n; ++i) {
    auto p = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list->_value._objects, i));

    if (lower) {
      // lower bound is set
      int compareResult = TRI_CompareValuesJson(lower, p, true);
      if (compareResult > 0 || (compareResult == 0 && ! includeLower)) {
        // element is bigger than lower bound
        continue;
      }
    }

    if (upper) {
      // upper bound is set
      int compareResult = TRI_CompareValuesJson(p, upper, true);
      if (compareResult > 0 || (compareResult == 0 && ! includeUpper)) {
        // element is smaller than upper bound
        continue;
      }
    }

    // element is between lower and upper bound

    TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p);
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief uniquify a sorted json list into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UniquifyArrayJson (TRI_json_t const* list) {
  TRI_json_t* last = nullptr;
  TRI_json_t* result;
  size_t i, n;

  TRI_ASSERT(list);
  TRI_ASSERT(list->_type == TRI_JSON_ARRAY);

  // create result list
  result = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE);

  if (result == nullptr) {
    return nullptr;
  }

  n = TRI_LengthVector(&list->_value._objects);

  for (i = 0; i < n; ++i) {
    auto p = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list->_value._objects, i));

    // don't push value if it is the same as the last value
    if (last == nullptr || TRI_CompareValuesJson(p, last, true) > 0) {
      TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p);

      // remember last element
      last = p;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the union of two sorted json lists into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_UnionizeListsJson (TRI_json_t const* list1,
                                   TRI_json_t const* list2,
                                   bool unique) {
  TRI_json_t* last = nullptr;
  TRI_json_t* result;
  size_t i1, i2;
  size_t n1, n2;

  TRI_ASSERT(list1);
  TRI_ASSERT(list1->_type == TRI_JSON_ARRAY);
  TRI_ASSERT(list2);
  TRI_ASSERT(list2->_type == TRI_JSON_ARRAY);

  n1 = TRI_LengthVector(&list1->_value._objects);
  n2 = TRI_LengthVector(&list2->_value._objects);

  // special cases for empty lists
  if (n1 == 0 && ! unique) {
    return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) list2);
  }

  if (n2 == 0 && ! unique) {
    return TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, (TRI_json_t*) list1);
  }

  // create result list
  result = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, n1 > n2 ? n1 : n2);

  if (result == nullptr) {
    return nullptr;
  }

  // reset positions
  i1 = 0;
  i2 = 0;

  // iterate over lists
  while (true) {
    // pointers to elements in both lists
    TRI_json_t* p1;
    TRI_json_t* p2;

    if (i1 < n1 && i2 < n2) {
      int compareResult;

      // both lists not yet exhausted
      p1 = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list1->_value._objects, i1));
      p2 = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list2->_value._objects, i2));
      compareResult = TRI_CompareValuesJson(p1, p2, true);

      if (compareResult < 0) {
        // left element is smaller
        if (! unique || last == nullptr || TRI_CompareValuesJson(p1, last, true) > 0) {
          TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
          last = p1;
        }
        ++i1;
      }
      else if (compareResult > 0) {
        // right element is smaller
        if (! unique || last == nullptr || TRI_CompareValuesJson(p2, last, true) > 0) {
          TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
          last = p2;
        }
        ++i2;
      }
      else {
        // both elements are equal
        if (! unique || last == nullptr || TRI_CompareValuesJson(p1, last, true) > 0) {
          TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
          last = p1;

          if (! unique) {
            TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
          }
        }
        ++i1;
        ++i2;
      }
    }
    else if (i1 < n1 && i2 >= n2) {
      // only right list is exhausted
      p1 = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list1->_value._objects, i1));

      if (! unique || last == nullptr || TRI_CompareValuesJson(p1, last, true) > 0) {
        TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
        last = p1;
      }
      ++i1;
    }
    else if (i1 >= n1 && i2 < n2) {
      // only left list is exhausted
      p2 = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list2->_value._objects, i2));

      if (! unique || last == nullptr || TRI_CompareValuesJson(p2, last, true) > 0) {
        TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
        last = p2;
      }
      ++i2;
    }
    else {
      // both lists exhausted, stop!
      break;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief create the intersection of two sorted json lists into a new list
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_IntersectListsJson (TRI_json_t const* list1,
                                    TRI_json_t const* list2,
                                    bool unique) {
  TRI_json_t* last = nullptr;
  TRI_json_t* result;
  size_t i1, i2;
  size_t n1, n2;

  TRI_ASSERT(list1);
  TRI_ASSERT(list1->_type == TRI_JSON_ARRAY);
  TRI_ASSERT(list2);
  TRI_ASSERT(list2->_type == TRI_JSON_ARRAY);

  n1 = TRI_LengthVector(&list1->_value._objects);
  n2 = TRI_LengthVector(&list2->_value._objects);

  // create result list
  result = TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, n1 > n2 ? n1 : n2);

  if (result == nullptr) {
    return nullptr;
  }

  // special case for empty lists
  if (n1 == 0 || n2 == 0) {
    return result;
  }

  // reset positions
  i1 = 0;
  i2 = 0;

  // iterate over lists
  while (i1 < n1 && i2 < n2) {
    // pointers to elements in both lists
    auto p1 = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list1->_value._objects, i1));
    auto p2 = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&list2->_value._objects, i2));

    int compareResult = TRI_CompareValuesJson(p1, p2, true);

    if (compareResult < 0) {
      // left element is smaller
      ++i1;
    }
    else if (compareResult > 0) {
      // right element is smaller
      ++i2;
    }
    else {
      // both elements are equal
      if (! unique || last == nullptr || TRI_CompareValuesJson(p1, last, true) > 0) {
        TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p1);
        last = p1;

        if (! unique) {
          TRI_PushBackArrayJson(TRI_UNKNOWN_MEM_ZONE, result, p2);
        }
      }
      ++i1;
      ++i2;
    }
  }

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief sorts a json list in place
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_SortArrayJson (TRI_json_t* list) {
  TRI_ASSERT(list != nullptr);
  TRI_ASSERT(list->_type == TRI_JSON_ARRAY);
    
  size_t const n = TRI_LengthVector(&list->_value._objects);

  if (n > 1) {
    // only sort if more than one value in list
    qsort(TRI_BeginVector(&list->_value._objects), n, sizeof(TRI_json_t), &CompareJson);
  }

  return list;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief checks if a JSON struct has duplicate attribute names
////////////////////////////////////////////////////////////////////////////////

bool TRI_HasDuplicateKeyJson (TRI_json_t const* object) {
  if (object && object->_type == TRI_JSON_OBJECT) {
    size_t const n = TRI_LengthVector(&object->_value._objects);
    const bool hasMultipleElements = (n > 2);

    // if we don't have attributes, we do not need to check for duplicates
    // if we only have one attribute, we don't need to check for duplicates in
    // the array, but we need to recursively validate the array values (if
    // array value itself is an array)
    if (n > 0) {
      TRI_associative_pointer_t hash;
      size_t i;

      if (hasMultipleElements) {
        TRI_InitAssociativePointer(&hash,
          TRI_UNKNOWN_MEM_ZONE,
          &TRI_HashStringKeyAssociativePointer,
          &TRI_HashStringKeyAssociativePointer,
          &TRI_EqualStringKeyAssociativePointer,
          0);
      }

      for (i = 0;  i < n; i += 2) {
        TRI_json_t* key;
        TRI_json_t* value;

        key = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&object->_value._objects, i));

        if (! TRI_IsStringJson(key)) {
          continue;
        }

        value = reinterpret_cast<TRI_json_t*>(TRI_AtVector(&object->_value._objects, i + 1));

        // recursively check sub-array elements
        if (value->_type == TRI_JSON_OBJECT && TRI_HasDuplicateKeyJson(value)) {
          // duplicate found in sub-array
          if (hasMultipleElements) {
            TRI_DestroyAssociativePointer(&hash);
          }

          return true;
        }

        if (hasMultipleElements) {
          void* previous = TRI_InsertKeyAssociativePointer(&hash, key->_value._string.data, key->_value._string.data, false);

          if (previous != nullptr) {
            // duplicate found
            TRI_DestroyAssociativePointer(&hash);

            return true;
          }
        }
      }

      if (hasMultipleElements) {
        TRI_DestroyAssociativePointer(&hash);
      }
    }
  }

  // no duplicate found
  return false;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief merge two JSON documents into one
////////////////////////////////////////////////////////////////////////////////

TRI_json_t* TRI_MergeJson (TRI_memory_zone_t* zone,
                           TRI_json_t const* lhs,
                           TRI_json_t const* rhs,
                           bool nullMeansRemove,
                           bool mergeObjects) {
  TRI_ASSERT(lhs->_type == TRI_JSON_OBJECT);
  TRI_ASSERT(rhs->_type == TRI_JSON_OBJECT);

  TRI_json_t* result = MergeRecursive(zone, lhs, rhs, nullMeansRemove, mergeObjects);

  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief computes a FNV hash for strings with a length
/// this function has an influence on how keys are distributed to shards
/// change with caution!
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashBlock (uint64_t hash, char const* buffer, size_t length) {
  uint64_t nMagicPrime = 0x00000100000001b3ULL;

  for (size_t j = 0; j < length; ++j) {
    hash ^= ((int8_t const*) buffer)[j];
    hash *= nMagicPrime;
  }
  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document, starting with a given
/// initial hash value. Note that a NULL pointer for json hashes to the
/// same value as a json pointer that points to a JSON value `null`.
////////////////////////////////////////////////////////////////////////////////

static uint64_t HashJsonRecursive (uint64_t hash, 
                                   TRI_json_t const* object) {
  if (nullptr == object) {
    return HashBlock(hash, "null", 4);   // strlen("null")
  }

  switch (object->_type) {
    case TRI_JSON_UNUSED: {
      return hash;
    }

    case TRI_JSON_NULL: {
      return HashBlock(hash, "null", 4);   // strlen("null")
    }

    case TRI_JSON_BOOLEAN: {
      if (object->_value._boolean) {
        return HashBlock(hash, "true", 4);  // strlen("true")
      }
      else {
        return HashBlock(hash, "false", 5);  // strlen("true")
      }
    }

    case TRI_JSON_NUMBER: {
      return HashBlock(hash, (char const*) &(object->_value._number), sizeof(object->_value._number));
    }

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      return HashBlock(hash,
                       object->_value._string.data,
                       object->_value._string.length);
    }

    case TRI_JSON_OBJECT: {
      hash = HashBlock(hash, "array", 5);   // strlen("array")
      size_t const n = TRI_LengthVector(&object->_value._objects);
      uint64_t tmphash = hash;
      for (size_t i = 0;  i < n;  i += 2) {
        auto subjson = static_cast<TRI_json_t const*>(TRI_AddressVector(&object->_value._objects, i));
        TRI_ASSERT(TRI_IsStringJson(subjson));
        tmphash ^= HashJsonRecursive(hash, subjson);
        subjson = static_cast<TRI_json_t const*>(TRI_AddressVector(&object->_value._objects, i + 1));
        tmphash ^= HashJsonRecursive(hash, subjson);
      }
      return tmphash;
    }

    case TRI_JSON_ARRAY: {
      hash = HashBlock(hash, "list", 4);   // strlen("list")
      size_t const n = TRI_LengthVector(&object->_value._objects);
      for (size_t i = 0;  i < n;  ++i) {
        auto subjson = static_cast<TRI_json_t const*>(TRI_AddressVector(&object->_value._objects, i));
        hash = HashJsonRecursive(hash, subjson);
      }
      return hash;
    }
  }
  return hash;   // never reached
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document. Note that a NULL pointer
/// for json hashes to the same value as a json pointer that points to a
/// JSON value `null`.
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashJson (TRI_json_t const* json) {
  return HashJsonRecursive(TRI_FnvHashBlockInitial(), json);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document depending on a list
/// of attributes. This is used for sharding to map documents to shards.
///
/// The attributes array `attributes` has to contain exactly `nrAttributes`
/// pointers to zero-terminated strings.
/// Note that all JSON values given for `json` that are not JSON arrays
/// hash to the same value, which is not the same value a JSON array gets
/// that does not contain any of the specified attributes.
/// If the flag `docComplete` is false, it is an error if the document
/// does not contain explicit values for all attributes. An error
/// is reported by setting *error to
/// TRI_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN instead of
/// TRI_ERROR_NO_ERROR. It is allowed to give NULL as error in which
/// case no error is reported.
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashJsonByAttributes (TRI_json_t const* json,
                                   char const *attributes[],
                                   int nrAttributes,
                                   bool docComplete,
                                   int* error) {
  if (error != nullptr) {
    *error = TRI_ERROR_NO_ERROR;
  }
  uint64_t hash = TRI_FnvHashBlockInitial();
  if (TRI_IsObjectJson(json)) {
    for (int i = 0; i < nrAttributes; i++) {
      TRI_json_t const* subjson = TRI_LookupObjectJson(json, attributes[i]);

      if (subjson == nullptr && ! docComplete && error != nullptr) {
        *error = TRI_ERROR_CLUSTER_NOT_ALL_SHARDING_ATTRIBUTES_GIVEN;
      }
      hash = HashJsonRecursive(hash, subjson);
    }
  }
  return hash;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief workhorse for fasthash computing
////////////////////////////////////////////////////////////////////////////////

static uint64_t FastHashJsonRecursive (uint64_t hash, 
                                       TRI_json_t const* object) {
  if (nullptr == object) {
    return fasthash64(static_cast<const void*>("null"), 4, hash);
  }

  switch (object->_type) {
    case TRI_JSON_UNUSED: {
      return hash;
    }

    case TRI_JSON_NULL: {
      return fasthash64(static_cast<const void*>("null"), 4, hash);
    }

    case TRI_JSON_BOOLEAN: {
      if (object->_value._boolean) {
        return fasthash64(static_cast<const void*>("true"), 4, hash);
      }
      return fasthash64(static_cast<const void*>("false"), 5, hash);
    }

    case TRI_JSON_NUMBER: {
      return fasthash64(static_cast<const void*>(&object->_value._number), sizeof(object->_value._number), hash);
    }

    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      return fasthash64(static_cast<const void*>(object->_value._string.data), object->_value._string.length, hash);
    }

    case TRI_JSON_OBJECT: {
      hash = fasthash64(static_cast<const void*>("object"), 6, hash);
      size_t const n = TRI_LengthVector(&object->_value._objects);
      uint64_t tmphash = hash;
      for (size_t i = 0;  i < n;  i += 2) {
        auto subjson = static_cast<TRI_json_t const*>(TRI_AddressVector(&object->_value._objects, i));
        TRI_ASSERT(TRI_IsStringJson(subjson));
        tmphash ^= FastHashJsonRecursive(hash, subjson);
        subjson = static_cast<TRI_json_t const*>(TRI_AddressVector(&object->_value._objects, i + 1));
        tmphash ^= FastHashJsonRecursive(hash, subjson);
      }
      return tmphash;
    }

    case TRI_JSON_ARRAY: {
      hash = fasthash64(static_cast<const void*>("array"), 5, hash);
      size_t const n = TRI_LengthVector(&object->_value._objects);
      for (size_t i = 0;  i < n;  ++i) {
        auto subjson = static_cast<TRI_json_t const*>(TRI_AddressVector(&object->_value._objects, i));
        hash = FastHashJsonRecursive(hash, subjson);
      }
    }
  }
  return hash;   // never reached
}


////////////////////////////////////////////////////////////////////////////////
/// @brief compute a hash value for a JSON document, using fasthash64.
/// This is slightly faster than the FNV-based hashing
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_FastHashJson (TRI_json_t const* json) {
  return FastHashJsonRecursive(0x012345678, json);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
