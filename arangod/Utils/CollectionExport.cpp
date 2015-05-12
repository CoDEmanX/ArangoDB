////////////////////////////////////////////////////////////////////////////////
/// @brief collection export result container
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

#include "Utils/CollectionExport.h"
#include "Basics/JsonHelper.h"
#include "Utils/CollectionGuard.h"
#include "Utils/CollectionReadLocker.h"
#include "Utils/transactions.h"
#include "VocBase/barrier.h"
#include "VocBase/compactor.h"
#include "VocBase/vocbase.h"

using namespace triagens::arango;

// -----------------------------------------------------------------------------
// --SECTION--                                            class CollectionExport
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

CollectionExport::CollectionExport (TRI_vocbase_t* vocbase,
                                    std::string const& name,
                                    Restrictions const& restrictions)
  : _guard(nullptr),
    _document(nullptr),
    _barrier(nullptr),
    _name(name),
    _resolver(vocbase),
    _restrictions(restrictions),
    _documents(nullptr) {

  // prevent the collection from being unloaded while the export is ongoing
  // this may throw
  _guard = new triagens::arango::CollectionGuard(vocbase, _name.c_str(), false);
  
  _document = _guard->collection()->_collection;
  TRI_ASSERT(_document != nullptr);
}

CollectionExport::~CollectionExport () {
  delete _documents;

  if (_barrier != nullptr) {
    TRI_FreeBarrier(_barrier);
  }

  delete _guard;
}
        
// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

void CollectionExport::run (uint64_t maxWaitTime) {
  // try to acquire the exclusive lock on the compaction
  while (! TRI_CheckAndLockCompactorVocBase(_document->_vocbase)) {
    // didn't get it. try again...
    usleep(5000);
  }
 
  // create a barrier under the compaction lock 
  _barrier = TRI_CreateBarrierElement(&_document->_barrierList);
  
  // release the lock
  TRI_UnlockCompactorVocBase(_document->_vocbase);

  // now we either have a barrier or not
  if (_barrier == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }


  TRI_ASSERT(_documents == nullptr);
  _documents = new std::vector<void const*>();
 
  { 
    static const uint64_t SleepTime = 10000;

    uint64_t tries = 0;
    uint64_t const maxTries = maxWaitTime / SleepTime;

    while (++tries < maxTries) {
      if (TRI_IsFullyCollectedDocumentCollection(_document)) {
        break;
      }
      usleep(SleepTime);
    }
  }

  {
    SingleCollectionReadOnlyTransaction trx(new StandaloneTransactionContext(), _document->_vocbase, _name);

    int res = trx.begin();

    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    size_t const n = static_cast<size_t>(_document->_primaryIndex._nrAlloc);
  
    _documents->reserve(static_cast<size_t>(_document->_primaryIndex._nrUsed));
  
    for (size_t i = 0; i < n; ++i) {
      auto ptr = _document->_primaryIndex._table[i];

      if (ptr != nullptr) {
        void const* marker = static_cast<TRI_doc_mptr_t const*>(ptr)->getDataPtr();

        // it is only safe to use the markers from the datafiles, not the WAL
        if (! TRI_IsWalDataMarkerDatafile(marker)) {
          _documents->emplace_back(marker);
        }
      }
    }

    trx.finish(res);
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
