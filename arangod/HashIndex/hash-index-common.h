////////////////////////////////////////////////////////////////////////////////
/// @brief hash index common definitions
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
/// @author Dr. Oreste Costa-Panaia
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_HASH_INDEX_HASH__INDEX_COMMON_H
#define ARANGODB_HASH_INDEX_HASH__INDEX_COMMON_H 1

#include "Basics/Common.h"
#include "VocBase/document-collection.h"

// -----------------------------------------------------------------------------
// --SECTION--                                              forward declarations
// -----------------------------------------------------------------------------

struct TRI_doc_mptr_t;
struct TRI_shaped_sub_s;

// -----------------------------------------------------------------------------
// --SECTION--                                                        HASH INDEX
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                                      public types
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index element
///
/// This structure is used for the elements of a unique hash index.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_hash_index_element_s {
  struct TRI_doc_mptr_t*                  _document;
  struct TRI_shaped_sub_s*                _subObjects;
}
TRI_hash_index_element_t;

////////////////////////////////////////////////////////////////////////////////
/// @brief hash index element
///
/// This structure is used for the elements of a non-unique hash index.
////////////////////////////////////////////////////////////////////////////////

typedef struct TRI_hash_index_element_multi_s {
  struct TRI_doc_mptr_t*                  _document;
  struct TRI_shaped_sub_s*                _subObjects;
  struct TRI_hash_index_element_multi_s*  _next;
}
TRI_hash_index_element_multi_t;

#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
