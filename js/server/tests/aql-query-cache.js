/*jshint globalstrict:false, strict:false, maxlen: 500 */
/*global assertEqual, assertTrue, assertFalse, AQL_EXECUTE, 
  AQL_QUERY_CACHE_PROPERTIES, AQL_QUERY_CACHE_INVALIDATE */

////////////////////////////////////////////////////////////////////////////////
/// @brief tests for query language, bind parameters
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2012, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

var jsunity = require("jsunity");
var db = require("org/arangodb").db;

////////////////////////////////////////////////////////////////////////////////
/// @brief test suite
////////////////////////////////////////////////////////////////////////////////

function ahuacatlQueryCacheTestSuite () {
  var cacheProperties;
  var c1, c2;

  return {

////////////////////////////////////////////////////////////////////////////////
/// @brief set up
////////////////////////////////////////////////////////////////////////////////

    setUp : function () {
      cacheProperties = AQL_QUERY_CACHE_PROPERTIES();
      AQL_QUERY_CACHE_INVALIDATE();

      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      c1 = db._create("UnitTestsAhuacatlQueryCache1");
      c2 = db._create("UnitTestsAhuacatlQueryCache2");
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief tear down
////////////////////////////////////////////////////////////////////////////////

    tearDown : function () {
      db._drop("UnitTestsAhuacatlQueryCache1");
      db._drop("UnitTestsAhuacatlQueryCache2");

      c1 = null;
      c2 = null;

      AQL_QUERY_CACHE_PROPERTIES(cacheProperties);
      AQL_QUERY_CACHE_INVALIDATE();
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test setting modes
////////////////////////////////////////////////////////////////////////////////

    testModes : function () {
      var result;

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "off" });
      assertEqual("off", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("off", result.mode);

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      assertEqual("on", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("on", result.mode);

      result = AQL_QUERY_CACHE_PROPERTIES({ mode: "demand" });
      assertEqual("demand", result.mode);
      result = AQL_QUERY_CACHE_PROPERTIES();
      assertEqual("demand", result.mode);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-deterministic queries
////////////////////////////////////////////////////////////////////////////////

    testNonDeterministicQueriesRandom : function () {
      var query = "FOR doc IN @@collection RETURN RAND()";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test non-deterministic queries
////////////////////////////////////////////////////////////////////////////////

    testNonDeterministicQueriesDocument : function () {
      var query = "FOR doc IN @@collection RETURN RAND()";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual(5, result.json.length);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single insert operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterInsertSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.save({ value: 2 }); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);

      assertEqual([ 1, 2 ], result.json);
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single update operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterUpdateSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      var doc = c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.update(doc, { value: 42 }); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 42 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 42 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after single remove operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterRemoveSingle : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result;

      c1.save({ value: 1 });

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1 ], result.json);

      c1.remove(c1.any()._key); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after truncate operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterTruncate : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 10; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.json);

      c1.truncate(); // this will invalidate cache

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);

      for (i = 1; i <= 10; ++i) {
        c1.save({ value: i });
      }
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after AQL insert operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterAqlInsert : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("INSERT { value: 9 } INTO @@collection", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5, 9 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after AQL update operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterAqlUpdate : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("FOR doc IN @@collection UPDATE doc._key WITH { value: doc.value + 1 } IN @@collection", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 2, 3, 4, 5, 6 ], result.json);
    },

////////////////////////////////////////////////////////////////////////////////
/// @brief test invalidation after AQL remove operation
////////////////////////////////////////////////////////////////////////////////

    testInvalidationAfterAqlRemove : function () {
      var query = "FOR doc IN @@collection SORT doc.value RETURN doc.value";
      var result, i;

      for (i = 1; i <= 5; ++i) {
        c1.save({ value: i });
      }

      AQL_QUERY_CACHE_PROPERTIES({ mode: "on" });
      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ 1, 2, 3, 4, 5 ], result.json);

      AQL_EXECUTE("FOR doc IN @@collection REMOVE doc._key IN @@collection", { "@collection" : c1.name() });

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertFalse(result.cached);
      assertEqual([ ], result.json);

      result = AQL_EXECUTE(query, { "@collection": c1.name() });
      assertTrue(result.cached);
      assertEqual([ ], result.json);
    }

// non-deterministic functions
// multi-collection queries

  };
}

////////////////////////////////////////////////////////////////////////////////
/// @brief executes the test suite
////////////////////////////////////////////////////////////////////////////////

jsunity.run(ahuacatlQueryCacheTestSuite);

return jsunity.done();

// Local Variables:
// mode: outline-minor
// outline-regexp: "^\\(/// @brief\\|/// @addtogroup\\|// --SECTION--\\|/// @page\\|/// @}\\)"
// End:
