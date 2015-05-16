////////////////////////////////////////////////////////////////////////////////
/// @brief associative array implementation
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
/// @author Dr. Frank Celler
/// @author Martin Schoenert
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2006-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "associative.h"

#include "Basics/hashes.h"
#include "Basics/tri-strings.h"

// -----------------------------------------------------------------------------
// --SECTION--                                                 ASSOCIATIVE ARRAY
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                   private defines
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initial number of elements in the array
////////////////////////////////////////////////////////////////////////////////

#define INITIAL_SIZE (11)

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElement (TRI_associative_array_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesR++;
#endif
  }

  // add a new element to the associative array
  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static void ResizeAssociativeArray (TRI_associative_array_t* array,
                                    uint32_t targetSize) {
  char * oldTable;
  uint32_t oldAlloc;
  uint32_t oldUsed;
  uint32_t j;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = targetSize;
#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  array->_table = static_cast<char*>(TRI_Allocate(array->_memoryZone, array->_nrAlloc * array->_elementSize, true));

  if (array->_table == NULL) {
    array->_nrAlloc = oldAlloc;
    array->_table = oldTable;

    return;
  }

  oldUsed = array->_nrUsed;
  array->_nrUsed = 0;

  for (j = 0; array->_nrUsed < oldUsed; j++) {
    if (! array->isEmptyElement(array, oldTable + j * array->_elementSize)) {
      AddNewElement(array, oldTable + j * array->_elementSize);
    }
  }

  TRI_Free(array->_memoryZone, oldTable);
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAssociativeArray (TRI_associative_array_t* array,
                              TRI_memory_zone_t* zone,
                              size_t elementSize,
                              uint64_t (*hashKey) (TRI_associative_array_t*, void*),
                              uint64_t (*hashElement) (TRI_associative_array_t*, void*),
                              void (*clearElement) (TRI_associative_array_t*, void*),
                              bool (*isEmptyElement) (TRI_associative_array_t*, void*),
                              bool (*isEqualKeyElement) (TRI_associative_array_t*, void*, void*),
                              bool (*isEqualElementElement) (TRI_associative_array_t*, void*, void*)) {
  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->clearElement = clearElement;
  array->isEmptyElement = isEmptyElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_memoryZone = zone;
  array->_elementSize = (uint32_t) elementSize;
  array->_nrAlloc = 0;
  array->_nrUsed  = 0;

  if (NULL == (array->_table = static_cast<char*>(TRI_Allocate(zone, array->_elementSize * INITIAL_SIZE, true)))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_nrAlloc = INITIAL_SIZE;

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;
#endif

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativeArray (TRI_associative_array_t* array) {
  if (array->_table != NULL) {
    TRI_Free(array->_memoryZone, array->_table);
    array->_table = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativeArray (TRI_memory_zone_t* zone, TRI_associative_array_t* array) {
  TRI_DestroyAssociativeArray(array);
  TRI_Free(zone, array);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyAssociativeArray (TRI_associative_array_t* array, void* key) {
  uint64_t hash;
  uint64_t i;

  if (array->_nrUsed == 0) {
    return NULL;
  }

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }

  // return whatever we found
  return array->_table + i * array->_elementSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given a key, return NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByKeyAssociativeArray (TRI_associative_array_t* array, void* key) {
  void* element;

  element = TRI_LookupByKeyAssociativeArray(array, key);

  if (element != NULL) {
    if (! array->isEmptyElement(array, element)) {
      return element;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementAssociativeArray (TRI_associative_array_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }

  // return whatever we found
  return array->_table + i * array->_elementSize;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief finds an element given an element, returns NULL if not found
////////////////////////////////////////////////////////////////////////////////

void* TRI_FindByElementAssociativeArray (TRI_associative_array_t* array, void* element) {
  void* element2;

  element2 = TRI_LookupByElementAssociativeArray(array, element);

  if (element2 != NULL) {
    if (! array->isEmptyElement(array, element2)) {
      return element2;
    }
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertElementAssociativeArray (TRI_associative_array_t* array, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return false;
  }

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  // if we found an element, return
  if (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (overwrite) {
      memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
    }

    return false;
  }

  // add a new element to the associative array
  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativeArray(array, (uint32_t) (2 * array->_nrAlloc + 1));
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_InsertKeyAssociativeArray (TRI_associative_array_t* array, void* key, void* element, bool overwrite) {
  uint64_t hash;
  uint64_t i;

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return false;
  }

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  // if we found an element, return
  if (! array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (overwrite) {
      memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
    }

    return false;
  }

  // add a new element to the associative array
  memcpy(array->_table + i * array->_elementSize, element, array->_elementSize);
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativeArray(array, (uint32_t) (2 * array->_nrAlloc + 1));
  }

  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveElementAssociativeArray (TRI_associative_array_t* array, void* element, void* old) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;

  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualElementElement(array, element, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return false
  if (array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (old != NULL) {
      memset(old, 0, array->_elementSize);
    }

    return false;
  }

  // remove item
  if (old != NULL) {
    memcpy(old, array->_table + i * array->_elementSize, array->_elementSize);
  }

  array->clearElement(array, array->_table + i * array->_elementSize);
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (! array->isEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = array->hashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      array->clearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

bool TRI_RemoveKeyAssociativeArray (TRI_associative_array_t* array, void* key, void* old) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;

  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table
  while (! array->isEmptyElement(array, array->_table + i * array->_elementSize)
         && ! array->isEqualKeyElement(array, key, array->_table + i * array->_elementSize)) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return false
  if (array->isEmptyElement(array, array->_table + i * array->_elementSize)) {
    if (old != NULL) {
      memset(old, 0, array->_elementSize);
    }

    return false;
  }

  // remove item
  if (old != NULL) {
    memcpy(old, array->_table + i * array->_elementSize, array->_elementSize);
  }

  array->clearElement(array, array->_table + i * array->_elementSize);
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (! array->isEmptyElement(array, array->_table + k * array->_elementSize)) {
    uint64_t j = array->hashElement(array, array->_table + k * array->_elementSize) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      memcpy(array->_table + i * array->_elementSize, array->_table + k * array->_elementSize, array->_elementSize);
      array->clearElement(array, array->_table + k * array->_elementSize);
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements from the array
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetLengthAssociativeArray (const TRI_associative_array_t* const array) {
  return array->_nrUsed;
}

// -----------------------------------------------------------------------------
// --SECTION--                                              ASSOCIATIVE POINTERS
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
////////////////////////////////////////////////////////////////////////////////

static void AddNewElementPointer (TRI_associative_pointer_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (array->_table[i] != NULL) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesR++;
#endif
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
////////////////////////////////////////////////////////////////////////////////

static bool ResizeAssociativePointer (TRI_associative_pointer_t* array,
                                      uint32_t targetSize) {
  void** oldTable;
  uint32_t oldAlloc;
  uint32_t j;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = targetSize;
#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  array->_table = static_cast<void**>(TRI_Allocate(array->_memoryZone, (size_t) (array->_nrAlloc * sizeof(void*)), true));

  if (array->_table == NULL) {
    array->_nrAlloc = oldAlloc;
    array->_table = oldTable;

    return false;
  }

  array->_nrUsed = 0;

  // table is already cleared by allocate, copy old data
  for (j = 0; j < oldAlloc; j++) {
    if (oldTable[j] != NULL) {
      AddNewElementPointer(array, oldTable[j]);
    }
  }

  TRI_Free(array->_memoryZone, oldTable);

  return true;
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAssociativePointer (TRI_associative_pointer_t* array,
                                TRI_memory_zone_t* zone,
                                uint64_t (*hashKey) (TRI_associative_pointer_t*, void const*),
                                uint64_t (*hashElement) (TRI_associative_pointer_t*, void const*),
                                bool (*isEqualKeyElement) (TRI_associative_pointer_t*, void const*, void const*),
                                bool (*isEqualElementElement) (TRI_associative_pointer_t*, void const*, void const*)) {
  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_memoryZone = zone;
  array->_nrAlloc = 0;
  array->_nrUsed  = 0;

  if (NULL == (array->_table = static_cast<void**>(TRI_Allocate(zone, sizeof(void*) * INITIAL_SIZE, true)))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_nrAlloc = INITIAL_SIZE;

#ifdef TRI_INTERNAL_STATS
  array->_nrFinds = 0;
  array->_nrAdds = 0;
  array->_nrRems = 0;
  array->_nrResizes = 0;
  array->_nrProbesF = 0;
  array->_nrProbesA = 0;
  array->_nrProbesD = 0;
  array->_nrProbesR = 0;
#endif

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativePointer (TRI_associative_pointer_t* array) {
  if (array->_table != NULL) {
    TRI_Free(array->_memoryZone, array->_table);
    array->_table = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativePointer (TRI_memory_zone_t* zone, TRI_associative_pointer_t* array) {
  TRI_DestroyAssociativePointer(array);
  TRI_Free(zone, array);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief General hash function that can be used to hash a pointer
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashPointerKeyAssociativePointer (TRI_associative_pointer_t* array,
                                               void const* ptr) {
  return TRI_FnvHashPointer(ptr, sizeof(void const*));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief General hash function that can be used to hash a key
////////////////////////////////////////////////////////////////////////////////

uint64_t TRI_HashStringKeyAssociativePointer (TRI_associative_pointer_t* array,
                                              void const* key) {
  return TRI_FnvHashString((char const*) key);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief General function to determine equality of two string values
////////////////////////////////////////////////////////////////////////////////

bool TRI_EqualStringKeyAssociativePointer (TRI_associative_pointer_t* array,
                                           void const* key,
                                           void const* element) {
  return TRI_EqualString((char*) key, (char*) element);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief reserves space in the array for extra elements
////////////////////////////////////////////////////////////////////////////////

bool TRI_ReserveAssociativePointer (TRI_associative_pointer_t* array,
                                    int32_t nrElements) {
  uint32_t targetSize = array->_nrUsed + nrElements;

  if (array->_nrAlloc < 2 * targetSize) {
    // we must resize
    return ResizeAssociativePointer(array, (uint32_t) (2 * targetSize) + 1);
  }

  // no seed to resize
  return true;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByKeyAssociativePointer (TRI_associative_pointer_t* array,
                                         void const* key) {
  uint64_t hash;
  uint64_t i;

  if (array->_nrUsed == 0) {
    return NULL;
  }

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesF++;
#endif
  }

  // return whatever we found
  return array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given an element
////////////////////////////////////////////////////////////////////////////////

void* TRI_LookupByElementAssociativePointer (TRI_associative_pointer_t* array,
                                             void const* element) {
  // compute the hash
  uint64_t const hash = array->hashElement(array, element);
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;
  i = k = hash % n;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrFinds++;
#endif

  // search the table
  for (; i < n && array->_table[i] != nullptr && ! array->isEqualElementElement(array, element, array->_table[i]); ++i);

  if (i == n) {
    for (i = 0; i < k && array->_table[i] != nullptr && ! array->isEqualElementElement(array, element, array->_table[i]); ++i);
  }

  // return whatever we found
  return array->_table[i];
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativePointer (TRI_associative_pointer_t* array,
                                           void* element,
                                           bool overwrite) {
  uint64_t hash;
  uint64_t i;
  void* old;

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // compute the hash
  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  old = array->_table[i];

  // if we found an element, return
  if (old != NULL) {
    if (overwrite) {
      array->_table[i] = element;
    }

    return old;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativePointer(array, (uint32_t) (2 * array->_nrAlloc) + 1);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativePointer (TRI_associative_pointer_t* array,
                                       void const* key,
                                       void* element,
                                       bool overwrite) {
  uint64_t hash;
  uint64_t i;
  void* old;

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed) {
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  old = array->_table[i];

  // if we found an element, return
  if (old != NULL) {
    if (overwrite) {
      array->_table[i] = element;
    }

    return old;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativePointer(array, (uint32_t) (2 * array->_nrAlloc) + 1);
  }

  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
/// returns a status code, and *found will contain a found element (if any)
////////////////////////////////////////////////////////////////////////////////

int TRI_InsertKeyAssociativePointer2 (TRI_associative_pointer_t* array,
                                      void const* key,
                                      void* element,
                                      void const** found) {
  uint64_t hash;
  uint64_t i;
  void* old;

  if (found != NULL) {
    *found = NULL;
  }

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  // compute the hash
  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrAdds++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesA++;
#endif
  }

  old = array->_table[i];

  // if we found an element, return
  if (old != NULL) {
    if (found != NULL) {
      *found = old;
    }

    return TRI_ERROR_NO_ERROR;
  }

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    if (! ResizeAssociativePointer(array, (uint32_t) (2 * array->_nrAlloc) + 1)) {
      return TRI_ERROR_OUT_OF_MEMORY;
    }

    // now we need to recalc the position
    i = hash % array->_nrAlloc;
    // search the table
    while (array->_table[i] != NULL && ! array->isEqualKeyElement(array, key, array->_table[i])) {
      i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
      array->_nrProbesA++;
#endif
    }
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementAssociativePointer (TRI_associative_pointer_t* array,
                                           void const* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  void* old;

  hash = array->hashElement(array, element);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return 0
  if (array->_table[i] == NULL) {
    return NULL;
  }

  // remove item
  old = array->_table[i];
  array->_table[i] = NULL;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = NULL;
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativePointer (TRI_associative_pointer_t* array,
                                       void const* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  void* old;

  hash = array->hashKey(array, key);
  i = hash % array->_nrAlloc;

#ifdef TRI_INTERNAL_STATS
  // update statistics
  array->_nrRems++;
#endif

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesD++;
#endif
  }

  // if we did not find such an item return false
  if (array->_table[i] == NULL) {
    return NULL;
  }

  // remove item
  old = array->_table[i];
  array->_table[i] = NULL;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = NULL;
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements from the array
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetLengthAssociativePointer (const TRI_associative_pointer_t* const array) {
  return array->_nrUsed;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                ASSOCIATIVE SYNCED
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief adds a new element
///
/// Note: this function must be called while the write-lock is held
////////////////////////////////////////////////////////////////////////////////

static void AddNewElementSynced (TRI_associative_synced_t* array, void* element) {
  uint64_t hash;
  uint64_t i;

  // compute the hash
  hash = array->hashElement(array, element);

  // search the table
  i = hash % array->_nrAlloc;

  while (array->_table[i] != NULL) {
    i = TRI_IncModU64(i, array->_nrAlloc);
#ifdef TRI_INTERNAL_STATS
    array->_nrProbesR++;
#endif
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief resizes the array
///
/// Note: this function must be called while the write-lock is held
////////////////////////////////////////////////////////////////////////////////

static void ResizeAssociativeSynced (TRI_associative_synced_t* array,
                                     uint32_t targetSize) {
  void** oldTable;
  uint32_t oldAlloc;
  uint32_t j;

  oldTable = array->_table;
  oldAlloc = array->_nrAlloc;

  array->_nrAlloc = targetSize;
#ifdef TRI_INTERNAL_STATS
  array->_nrResizes++;
#endif

  array->_table = static_cast<void**>(TRI_Allocate(array->_memoryZone, array->_nrAlloc * sizeof(void*), true));

  if (array->_table == NULL) {
    array->_nrAlloc = oldAlloc;
    array->_table = oldTable;

    return;
  }

  array->_nrUsed = 0;

  // table is already cleared by allocate, copy old data
  for (j = 0; j < oldAlloc; j++) {
    if (oldTable[j] != NULL) {
      AddNewElementSynced(array, oldTable[j]);
    }
  }

  TRI_Free(array->_memoryZone, oldTable);
}

// -----------------------------------------------------------------------------
// --SECTION--                                      constructors and destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief initialises an array
////////////////////////////////////////////////////////////////////////////////

int TRI_InitAssociativeSynced (TRI_associative_synced_t* array,
                               TRI_memory_zone_t* zone,
                               uint64_t (*hashKey) (TRI_associative_synced_t*, void const*),
                               uint64_t (*hashElement) (TRI_associative_synced_t*, void const*),
                               bool (*isEqualKeyElement) (TRI_associative_synced_t*, void const*, void const*),
                               bool (*isEqualElementElement) (TRI_associative_synced_t*, void const*, void const*)) {
  array->hashKey = hashKey;
  array->hashElement = hashElement;
  array->isEqualKeyElement = isEqualKeyElement;
  array->isEqualElementElement = isEqualElementElement;

  array->_memoryZone = zone;
  array->_nrAlloc = 0;
  array->_nrUsed  = 0;

  if (NULL == (array->_table = static_cast<void**>(TRI_Allocate(zone, sizeof(void*) * INITIAL_SIZE, true)))) {
    return TRI_ERROR_OUT_OF_MEMORY;
  }

  array->_nrAlloc = INITIAL_SIZE;

  TRI_InitReadWriteLock(&array->_lock);

  return TRI_ERROR_NO_ERROR;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array, but does not free the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_DestroyAssociativeSynced (TRI_associative_synced_t* array) {
  if (array->_table != NULL) {
    TRI_Free(array->_memoryZone, array->_table);
    array->_table = NULL;
  }

  TRI_DestroyReadWriteLock(&array->_lock);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroys an array and frees the pointer
////////////////////////////////////////////////////////////////////////////////

void TRI_FreeAssociativeSynced (TRI_memory_zone_t* zone, TRI_associative_synced_t* array) {
  TRI_DestroyAssociativeSynced(array);
  TRI_Free(zone, array);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given a key
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByKeyAssociativeSynced (TRI_associative_synced_t* array,
                                              void const* key) {
  void const* result;

  // compute the hash
  uint64_t const hash = array->hashKey(array, key);

  // search the table
  TRI_ReadLockReadWriteLock(&array->_lock);
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = hash % n;

  for (; i < n && array->_table[i] != nullptr && ! array->isEqualKeyElement(array, key, array->_table[i]); ++i);

  if (i == n) {
    for (i = 0; i < k && array->_table[i] != nullptr && ! array->isEqualKeyElement(array, key, array->_table[i]); ++i);
  }

  result = array->_table[i];

  TRI_ReadUnlockReadWriteLock(&array->_lock);

  // return whatever we found
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief looks up an element given an element
////////////////////////////////////////////////////////////////////////////////

void const* TRI_LookupByElementAssociativeSynced (TRI_associative_synced_t* array,
                                                  void const* element) {
  void const* result;

  // compute the hash
  uint64_t const hash = array->hashElement(array, element);

  // search the table
  TRI_ReadLockReadWriteLock(&array->_lock);
  uint64_t const n = array->_nrAlloc;
  uint64_t i, k;

  i = k = hash % n;

  for (; i < n && array->_table[i] != nullptr && ! array->isEqualElementElement(array, element, array->_table[i]); ++i);

  if (i == n) {
    for (i = 0; i < k && array->_table[i] != nullptr && ! array->isEqualElementElement(array, element, array->_table[i]); ++i);
  }

  result = array->_table[i];

  TRI_ReadUnlockReadWriteLock(&array->_lock);

  // return whatever we found
  return result;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertElementAssociativeSynced (TRI_associative_synced_t* array,
                                          void* element,
                                          bool overwrite) {
  uint64_t hash;
  uint64_t i;
  void* old;

  // compute the hash
  hash = array->hashElement(array, element);

  TRI_WriteLockReadWriteLock(&array->_lock);

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed && ! overwrite) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  i = hash % array->_nrAlloc;

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
  }

  old = array->_table[i];

  // if we found an element, return
  if (old != NULL) {
    if (overwrite) {
      array->_table[i] = element;
    }
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return old;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativeSynced(array, (uint32_t) (2 * array->_nrAlloc + 1));
  }

  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief adds an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_InsertKeyAssociativeSynced (TRI_associative_synced_t* array,
                                      void const* key,
                                      void* element,
                                      bool overwrite) {
  uint64_t hash;
  uint64_t i;
  void* old;

  // compute the hash
  hash = array->hashKey(array, key);

  // search the table
  TRI_WriteLockReadWriteLock(&array->_lock);

  // check for out-of-memory
  if (array->_nrAlloc == array->_nrUsed && ! overwrite) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    TRI_set_errno(TRI_ERROR_OUT_OF_MEMORY);
    return NULL;
  }

  i = hash % array->_nrAlloc;

  while (array->_table[i] != NULL && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
  }

  old = array->_table[i];

  // if we found an element, return
  if (old != NULL) {
    if (overwrite) {
      array->_table[i] = element;
    }
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return old;
  }

  // add a new element to the associative array
  array->_table[i] = element;
  array->_nrUsed++;

  // if we were adding and the table is more than half full, extend it
  if (array->_nrAlloc < 2 * array->_nrUsed) {
    ResizeAssociativeSynced(array, (uint32_t) (2 * array->_nrAlloc + 1));
  }

  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an element from the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveElementAssociativeSynced (TRI_associative_synced_t* array,
                                          void const* element) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  void* old;

  hash = array->hashElement(array, element);

  TRI_WriteLockReadWriteLock(&array->_lock);
  i = hash % array->_nrAlloc;

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualElementElement(array, element, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
  }

  // if we did not find such an item return 0
  if (array->_table[i] == NULL) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return NULL;
  }

  // remove item
  old = array->_table[i];
  array->_table[i] = NULL;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = NULL;
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief removes an key/element to the array
////////////////////////////////////////////////////////////////////////////////

void* TRI_RemoveKeyAssociativeSynced (TRI_associative_synced_t* array,
                                      void const* key) {
  uint64_t hash;
  uint64_t i;
  uint64_t k;
  void* old;

  hash = array->hashKey(array, key);

  TRI_WriteLockReadWriteLock(&array->_lock);
  i = hash % array->_nrAlloc;

  // search the table
  while (array->_table[i] != NULL && ! array->isEqualKeyElement(array, key, array->_table[i])) {
    i = TRI_IncModU64(i, array->_nrAlloc);
  }

  // if we did not find such an item return false
  if (array->_table[i] == NULL) {
    TRI_WriteUnlockReadWriteLock(&array->_lock);
    return NULL;
  }

  // remove item
  old = array->_table[i];
  array->_table[i] = NULL;
  array->_nrUsed--;

  // and now check the following places for items to move here
  k = TRI_IncModU64(i, array->_nrAlloc);

  while (array->_table[k] != NULL) {
    uint64_t j = array->hashElement(array, array->_table[k]) % array->_nrAlloc;

    if ((i < k && !(i < j && j <= k)) || (k < i && !(i < j || j <= k))) {
      array->_table[i] = array->_table[k];
      array->_table[k] = NULL;
      i = k;
    }

    k = TRI_IncModU64(k, array->_nrAlloc);
  }

  // return success
  TRI_WriteUnlockReadWriteLock(&array->_lock);
  return old;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the number of elements from the array
////////////////////////////////////////////////////////////////////////////////

size_t TRI_GetLengthAssociativeSynced (TRI_associative_synced_t* const array) {
  uint32_t result;

  TRI_ReadLockReadWriteLock(&array->_lock);
  result = array->_nrUsed;
  TRI_ReadUnlockReadWriteLock(&array->_lock);

  return (size_t) result;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
