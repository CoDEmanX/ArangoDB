////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, C++ implementation of AQL functions
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

#include "Aql/Functions.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"
#include "Basics/JsonHelper.h"
#include "Basics/json-utilities.h"
#include "Basics/StringBuffer.h"
#include "Rest/SslInterface.h"

using namespace triagens::aql;
using Json = triagens::basics::Json;

////////////////////////////////////////////////////////////////////////////////
/// @brief register warning
////////////////////////////////////////////////////////////////////////////////
            
static void RegisterWarning (triagens::aql::Query* query,
                             char const* functionName,
                             int code) {
  std::string msg;

  if (code == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH) {
    msg = triagens::basics::Exception::FillExceptionString(code, functionName);
  }
  else {
    msg.append("in function '");
    msg.append(functionName);
    msg.append("()': ");
    msg.append(TRI_errno_string(code));
  }

  query->registerWarning(code, msg.c_str());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief register usage of an invalid function argument
////////////////////////////////////////////////////////////////////////////////
            
static void RegisterInvalidArgumentWarning (triagens::aql::Query* query,
                                            char const* functionName) {
  RegisterWarning(query, functionName, TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief extract attribute names from the arguments
////////////////////////////////////////////////////////////////////////////////

static void ExtractKeys (std::unordered_set<std::string>& names,
                         triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const& parameters,
                         size_t startParameter,
                         char const* functionName) {  
  size_t const n = parameters.arraySize();

  for (size_t i = startParameter; i < n; ++i) {
    Json param(parameters.extractArrayMember(trx, collection, i, false));

    if (param.isString()) {
      TRI_json_t const* json = param.json();
      names.emplace(std::string(json->_value._string.data, json->_value._string.length - 1));
    }
    else if (param.isNumber()) {
      TRI_json_t const* json = param.json();
      double number = json->_value._number;

      if (std::isnan(number) || number == HUGE_VAL || number == -HUGE_VAL) {
        names.emplace("null");
      } 
      else {
        char buffer[24];
        int length = fpconv_dtoa(number, &buffer[0]);
        names.emplace(std::string(&buffer[0], static_cast<size_t>(length)));
      }
    }
    else if (param.isArray()) {
      TRI_json_t const* p = param.json();

      size_t const n2 = param.size();
      for (size_t j = 0; j < n2; ++j) {
        auto v = static_cast<TRI_json_t const*>(TRI_AtVector(&p->_value._objects, j));
        if (TRI_IsStringJson(v)) {
          names.emplace(std::string(v->_value._string.data, v->_value._string.length - 1));
        }
        else {
          RegisterInvalidArgumentWarning(query, functionName);
        }
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief append the JSON value to a string buffer
////////////////////////////////////////////////////////////////////////////////

static void AppendAsString (triagens::basics::StringBuffer& buffer,
                            TRI_json_t const* json) {
  TRI_json_type_e const type = (json == nullptr ? TRI_JSON_UNUSED : json->_type);

  switch (type) {
    case TRI_JSON_UNUSED: 
    case TRI_JSON_NULL: {
      buffer.appendText("null", strlen("null"));
      break;
    }
    case TRI_JSON_BOOLEAN: {
      if (json->_value._boolean) {
        buffer.appendText("true", strlen("true"));
      }
      else {
        buffer.appendText("false", strlen("false"));
      }
      break;
    }
    case TRI_JSON_NUMBER: {
      buffer.appendDecimal(json->_value._number);
      break;
    }
    case TRI_JSON_STRING:
    case TRI_JSON_STRING_REFERENCE: {
      buffer.appendText(json->_value._string.data, json->_value._string.length - 1);
      break;
    }
    case TRI_JSON_ARRAY: {
      size_t const n = TRI_LengthArrayJson(json);
      for (size_t i = 0; i < n; ++i) {
        if (i > 0) {
          buffer.appendChar(',');
        }
        AppendAsString(buffer, static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, i)));
      }
      break;
    }
    case TRI_JSON_OBJECT: {
      buffer.appendText("[object Object]", strlen("[object Object]"));
      break;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NULL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNull (triagens::aql::Query*, 
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNull()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_BOOL
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsBool (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isBoolean()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_NUMBER
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsNumber (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isNumber()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_STRING
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsString (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isString()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_ARRAY
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsArray (triagens::aql::Query*,
                             triagens::arango::AqlTransaction* trx,
                             TRI_document_collection_t const* collection,
                             AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isArray()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function IS_OBJECT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::IsObject (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));
  return AqlValue(new Json(j.isObject()));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function LENGTH
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Length (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json j(parameters.extractArrayMember(trx, collection, 0, false));

  TRI_json_t const* json = j.json();
  size_t length = 0;

  if (json != nullptr) {
    switch (json->_type) {
      case TRI_JSON_UNUSED:
      case TRI_JSON_NULL: {
        length = 0;
        break;
      }

      case TRI_JSON_BOOLEAN: {
        length = (json->_value._boolean ? 1 : 0);
        break;
      }

      case TRI_JSON_NUMBER: {
        if (std::isnan(json->_value._number) ||
            ! std::isfinite(json->_value._number)) {
          // invalid value
          length = strlen("null");
        }
        else {
          // convert to a string representation of the number
          char buffer[24];
          length = static_cast<size_t>(fpconv_dtoa(json->_value._number, buffer));
        }
        break;
      }

      case TRI_JSON_STRING:
      case TRI_JSON_STRING_REFERENCE: {
        // return number of characters (not bytes) in string
        length = TRI_CharLengthUtf8String(json->_value._string.data);
        break;
      }

      case TRI_JSON_OBJECT: {
        // return number of attributes
        length = TRI_LengthVector(&json->_value._objects) / 2;
        break;
      }

      case TRI_JSON_ARRAY: {
        // return list length
        length = TRI_LengthArrayJson(json);
        break;
      }
    }
  }

  return AqlValue(new Json(static_cast<double>(length)));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function CONCAT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Concat (triagens::aql::Query*,
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE, 24);

  size_t const n = parameters.arraySize();

  for (size_t i = 0; i < n; ++i) {
    Json member = parameters.at(trx, i);

    if (member.isEmpty() || member.isNull()) {
      continue;
    }
      
    TRI_json_t const* json = member.json();
    
    if (member.isArray()) {
      // append each member individually
      size_t const subLength = TRI_LengthArrayJson(json);

      for (size_t j = 0; j < subLength; ++j) {
        auto sub = static_cast<TRI_json_t const*>(TRI_AtVector(&json->_value._objects, j));

        if (sub == nullptr || sub->_type == TRI_JSON_NULL) {
          continue;
        }

        AppendAsString(buffer, sub);
      }
    }
    else {
      // convert member to a string and append
      AppendAsString(buffer, json);
    }
  }
  
  // steal the StringBuffer's char* pointer so we can avoid copying data around
  // multiple times
  size_t length = buffer.length();
  std::unique_ptr<TRI_json_t> j(TRI_CreateStringJson(TRI_UNKNOWN_MEM_ZONE, buffer.steal(), length));

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function PASSTHRU
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Passthru (triagens::aql::Query*,
                              triagens::arango::AqlTransaction* trx,
                              TRI_document_collection_t const* collection,
                              AqlValue const parameters) {

  Json j(parameters.extractArrayMember(trx, collection, 0, true));
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.json());
  j.steal();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNSET
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unset (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           TRI_document_collection_t const* collection,
                           AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isObject()) {
    RegisterInvalidArgumentWarning(query, "UNSET");
    return AqlValue(new Json(Json::Null));
  }
 
  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, collection, parameters, 1, "UNSET");


  // create result object
  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthVector(&valueJson->_value._objects);

  size_t size;
  if (names.size() >= n / 2) {
    size = 4; 
  }
  else {
    size = (n / 2) - names.size(); 
  }

  std::unique_ptr<TRI_json_t> j(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, size));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i + 1));

    if (TRI_IsStringJson(key) && 
        names.find(key->_value._string.data) == names.end()) {
      auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value);

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      } 

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, j.get(), key->_value._string.data, copy);
    }
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function KEEP
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Keep (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          TRI_document_collection_t const* collection,
                          AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isObject()) {
    RegisterInvalidArgumentWarning(query, "KEEP");
    return AqlValue(new Json(Json::Null));
  }
 
  std::unordered_set<std::string> names;
  ExtractKeys(names, query, trx, collection, parameters, 1, "KEEP");


  // create result object
  std::unique_ptr<TRI_json_t> j(TRI_CreateObjectJson(TRI_UNKNOWN_MEM_ZONE, names.size()));

  if (j == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthVector(&valueJson->_value._objects);

  for (size_t i = 0; i < n; i += 2) {
    auto key = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i + 1));

    if (TRI_IsStringJson(key) && 
        names.find(key->_value._string.data) != names.end()) {
      auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value);

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      } 

      TRI_Insert3ObjectJson(TRI_UNKNOWN_MEM_ZONE, j.get(), key->_value._string.data, copy);
    }
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, j.get());
  j.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MERGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Merge (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           TRI_document_collection_t const* collection,
                           AqlValue const parameters) {
  size_t const n = parameters.arraySize();

  if (n == 0) {
    // no parameters
    return AqlValue(new Json(Json::Object));
  }

  // use the first argument as the preliminary result
  Json initial(parameters.extractArrayMember(trx, collection, 0, true));

  if (! initial.isObject()) {
    RegisterInvalidArgumentWarning(query, "MERGE");
    return AqlValue(new Json(Json::Null));
  }

  std::unique_ptr<TRI_json_t> result(initial.steal());

  // now merge in all other arguments
  for (size_t i = 1; i < n; ++i) {
    Json param(parameters.extractArrayMember(trx, collection, i, false));

    if (! param.isObject()) {
      RegisterInvalidArgumentWarning(query, "MERGE");
      return AqlValue(new Json(Json::Null));
    }
 
    auto merged = TRI_MergeJson(TRI_UNKNOWN_MEM_ZONE, result.get(), param.json(), false, true);

    if (merged == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    result.reset(merged);
  } 

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function HAS
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Has (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const parameters) {
  size_t const n = parameters.arraySize();

  if (n < 2) {
    // no parameters
    return AqlValue(new Json(false));
  }
    
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isObject()) {
    // not an object
    return AqlValue(new Json(false));
  }
 
  // process name parameter 
  Json name(parameters.extractArrayMember(trx, collection, 1, false));

  char const* p;

  if (! name.isString()) {
    triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
    AppendAsString(buffer, name.json());
    p = buffer.c_str();
  }
  else {
    p = name.json()->_value._string.data;
  }
 
  bool const hasAttribute = (TRI_LookupObjectJson(value.json(), p) != nullptr);
  return AqlValue(new Json(hasAttribute));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MIN
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Min (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "MIN", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);
  TRI_json_t const* minValue = nullptr;;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (minValue == nullptr ||
        TRI_CompareValuesJson(value, minValue) < 0) {
      minValue = value;
    }
  } 

  if (minValue != nullptr) {
    std::unique_ptr<TRI_json_t> result(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, minValue));
    
    if (result != nullptr) {
      auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
      result.release();
      return AqlValue(jr);
    }
  }

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MAX
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Max (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "MAX", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);
  TRI_json_t const* maxValue = nullptr;;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (maxValue == nullptr ||
        TRI_CompareValuesJson(value, maxValue) > 0) {
      maxValue = value;
    }
  } 

  if (maxValue != nullptr) {
    std::unique_ptr<TRI_json_t> result(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, maxValue));
    
    if (result != nullptr) {
      auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
      result.release();
      return AqlValue(jr);
    }
  }

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SUM
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sum (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "SUM", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);
  double sum = 0.0;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (! TRI_IsNumberJson(value)) {
      RegisterInvalidArgumentWarning(query, "SUM");
      return AqlValue(new Json(Json::Null));
    }

    // got a numeric value
    double const number = value->_value._number;

    if (! std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
    } 
  } 

  if (! std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    return AqlValue(new Json(sum));
  } 

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function AVERAGE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Average (triagens::aql::Query* query,
                             triagens::arango::AqlTransaction* trx,
                             TRI_document_collection_t const* collection,
                             AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "AVERAGE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);
  double sum = 0.0;
  size_t count = 0;

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AtVector(&valueJson->_value._objects, i));

    if (TRI_IsNullJson(value)) {
      continue;
    }

    if (! TRI_IsNumberJson(value)) {
      RegisterInvalidArgumentWarning(query, "AVERAGE");
      return AqlValue(new Json(Json::Null));
    }

    // got a numeric value
    double const number = value->_value._number;

    if (! std::isnan(number) && number != HUGE_VAL && number != -HUGE_VAL) {
      sum += number;
      ++count;
    } 
  } 

  if (count > 0 && 
      ! std::isnan(sum) && sum != HUGE_VAL && sum != -HUGE_VAL) {
    return AqlValue(new Json(sum / static_cast<size_t>(count)));
  } 

  return AqlValue(new Json(Json::Null));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function MD5
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Md5 (triagens::aql::Query* query,
                         triagens::arango::AqlTransaction* trx,
                         TRI_document_collection_t const* collection,
                         AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));
    
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  AppendAsString(buffer, value.json());
  
  // create md5
  char hash[17]; 
  char* p = &hash[0];
  size_t length;

  triagens::rest::SslInterface::sslMD5(buffer.c_str(), buffer.length(), p, length);

  // as hex
  char hex[33];
  p = &hex[0];

  triagens::rest::SslInterface::sslHEX(hash, 16, p, length);

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, hex, 32));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function SHA1
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Sha1 (triagens::aql::Query* query,
                          triagens::arango::AqlTransaction* trx,
                          TRI_document_collection_t const* collection,
                          AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));
    
  triagens::basics::StringBuffer buffer(TRI_UNKNOWN_MEM_ZONE);
  AppendAsString(buffer, value.json());
  
  // create sha1
  char hash[21];
  char* p = &hash[0];
  size_t length;

  triagens::rest::SslInterface::sslSHA1(buffer.c_str(), buffer.length(), p, length);

  // as hex
  char hex[41];
  p = &hex[0];

  triagens::rest::SslInterface::sslHEX(hash, 20, p, length);

  return AqlValue(new Json(TRI_UNKNOWN_MEM_ZONE, hex, 40));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNIQUE
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Unique (triagens::aql::Query* query,
                            triagens::arango::AqlTransaction* trx,
                            TRI_document_collection_t const* collection,
                            AqlValue const parameters) {
  Json value(parameters.extractArrayMember(trx, collection, 0, false));

  if (! value.isArray()) {
    // not an array
    RegisterWarning(query, "UNIQUE", TRI_ERROR_QUERY_ARRAY_EXPECTED);
    return AqlValue(new Json(Json::Null));
  }

  TRI_json_t const* valueJson = value.json();
  size_t const n = TRI_LengthArrayJson(valueJson);

  std::unordered_set<TRI_json_t const*, triagens::basics::JsonHash, triagens::basics::JsonEqual> values(
    512, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  for (size_t i = 0; i < n; ++i) {
    auto value = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, i));

    if (value == nullptr) {
      continue;
    }

    values.emplace(value); 
  } 

  std::unique_ptr<TRI_json_t> result(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, values.size()));
 
  for (auto const& it : values) {
    auto copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it);

    if (copy == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
 
    TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), copy); 
  }
      
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Union (triagens::aql::Query* query,
                           triagens::arango::AqlTransaction* trx,
                           TRI_document_collection_t const* collection,
                           AqlValue const parameters) {
  std::unique_ptr<TRI_json_t> result(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, 16));

  size_t const n = parameters.arraySize();

  for (size_t i = 0; i < n; ++i) {
    Json value(parameters.extractArrayMember(trx, collection, i, false));

    if (! value.isArray()) {
      // not an array
      RegisterWarning(query, "UNION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
      return AqlValue(new Json(Json::Null));
    }

    TRI_json_t const* valueJson = value.json();
    size_t const nrValues = TRI_LengthArrayJson(valueJson);

    if (TRI_ReserveVector(&(result.get()->_value._objects), nrValues) != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }

    TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
    
    // this passes ownership for the JSON contens into result
    for (size_t j = 0; j < nrValues; ++j) {
      TRI_json_t* copy = TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, TRI_LookupArrayJson(valueJson, j));

      if (copy == nullptr) {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
      }
    
      TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), copy);

      TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
    } 
  } 
      
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function UNION_DISTINCT
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::UnionDistinct (triagens::aql::Query* query,
                                   triagens::arango::AqlTransaction* trx,
                                   TRI_document_collection_t const* collection,
                                   AqlValue const parameters) {
  std::unordered_set<TRI_json_t*, triagens::basics::JsonHash, triagens::basics::JsonEqual> values(
    512, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  auto freeValues = [&values] () -> void {
    for (auto& it : values) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it);
    }
  };

  std::unique_ptr<TRI_json_t> result;
  size_t const n = parameters.arraySize();

  try {
    for (size_t i = 0; i < n; ++i) {
      Json value(parameters.extractArrayMember(trx, collection, i, false));

      if (! value.isArray()) {
        // not an array
        freeValues();
        RegisterWarning(query, "UNION_DISTINCT", TRI_ERROR_QUERY_ARRAY_EXPECTED);
        return AqlValue(new Json(Json::Null));
      }

      TRI_json_t const* valueJson = value.json();
      size_t const nrValues = TRI_LengthArrayJson(valueJson);

      for (size_t j = 0; j < nrValues; ++j) {
        auto value = static_cast<TRI_json_t*>(TRI_AddressVector(&valueJson->_value._objects, j));

        if (values.find(value) == values.end()) { 
          std::unique_ptr<TRI_json_t> copy(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value));

          if (copy == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
      
          TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          values.emplace(copy.get());
          copy.release();
        }
      }
    }

    result.reset(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, values.size()));

    if (result == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
          
    TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
   
    for (auto const& it : values) {
      TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), it); 
    }

  }
  catch (...) {  
    freeValues();
    throw;
  }
    
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
      
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief function INTERSECTION
////////////////////////////////////////////////////////////////////////////////

AqlValue Functions::Intersection (triagens::aql::Query* query,
                                  triagens::arango::AqlTransaction* trx,
                                  TRI_document_collection_t const* collection,
                                  AqlValue const parameters) {
  std::unordered_map<TRI_json_t*, size_t, triagens::basics::JsonHash, triagens::basics::JsonEqual> values(
    512, 
    triagens::basics::JsonHash(), 
    triagens::basics::JsonEqual()
  );

  auto freeValues = [&values] () -> void {
    for (auto& it : values) {
      TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it.first);
    }
    values.clear();
  };

  std::unique_ptr<TRI_json_t> result;
  size_t const n = parameters.arraySize();

  try {
    for (size_t i = 0; i < n; ++i) {
      Json value(parameters.extractArrayMember(trx, collection, i, false));

      if (! value.isArray()) {
        // not an array
        freeValues();
        RegisterWarning(query, "INTERSECTION", TRI_ERROR_QUERY_ARRAY_EXPECTED);
        return AqlValue(new Json(Json::Null));
      }

      TRI_json_t const* valueJson = value.json();
      size_t const nrValues = TRI_LengthArrayJson(valueJson);

      for (size_t j = 0; j < nrValues; ++j) {
        auto value = static_cast<TRI_json_t const*>(TRI_AddressVector(&valueJson->_value._objects, j));

        if (i == 0) {
          // round one
          std::unique_ptr<TRI_json_t> copy(TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, value));

          if (copy == nullptr) {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
          }
    
          TRI_IF_FAILURE("AqlFunctions::OutOfMemory1") {
            THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
          }

          auto r = values.emplace(copy.get(), 1);
 
          if (r.second) {
            // successfully inserted
            copy.release();
          }
        }
        else {
          // check if we have seen the same element before
          auto it = values.find(const_cast<TRI_json_t*>(value));

          if (it != values.end()) {
            // already seen
            TRI_ASSERT((*it).second > 0);
            ++((*it).second);
          }
        }
      }
    }
 
    // count how many valid we have 
    size_t total = 0;

    for (auto const& it : values) {
      if (it.second == n) {
        ++total;
      }
    }

    result.reset(TRI_CreateArrayJson(TRI_UNKNOWN_MEM_ZONE, total));

    if (result == nullptr) {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
    }
          
    TRI_IF_FAILURE("AqlFunctions::OutOfMemory2") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }
   
    for (auto& it : values) {
      if (it.second == n) {
        TRI_PushBack3ArrayJson(TRI_UNKNOWN_MEM_ZONE, result.get(), it.first); 
      }
      else {
        TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, it.first);
      }
    }
    values.clear();
   
  } 
  catch (...) {
    freeValues();
    throw;
  }
    
  TRI_IF_FAILURE("AqlFunctions::OutOfMemory3") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }
      
  auto jr = new Json(TRI_UNKNOWN_MEM_ZONE, result.get());
  result.release();
  return AqlValue(jr);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
