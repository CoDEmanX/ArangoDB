////////////////////////////////////////////////////////////////////////////////
/// @brief index watermarks
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
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2011-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_INDEXES_INDEXWATERMARKS_H
#define ARANGODB_INDEXES_INDEXWATERMARKS_H 1

#if 0
#include "Basics/Common.h"

// -----------------------------------------------------------------------------
// --SECTION--                                            struct IndexWatermarks
// -----------------------------------------------------------------------------

namespace triagens {
  namespace arango {

    struct IndexWatermarks {
      IndexWatermarks ()
        : initialFillFactor(DefaultInitialFillFactor),
          lowWatermark(DefaultLowWatermark),
          highWatermark(DefaultHighWatermark) {
      }

      static void SetDefaults (IndexWatermarks const&);

      double initialFillFactor;
      double lowWatermark;
      double highWatermark;

      static double DefaultInitialFillFactor;
      static double DefaultLowWatermark;
      static double DefaultHighWatermark;
    };

  }
}

#endif
#endif

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
