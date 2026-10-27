// The MIT License (MIT)
//
// Copyright (c) 2024-2026 Quarkifi Technologies Pvt Ltd
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// CRUD tests for the public QuarkDB API, backed by the in-memory SPIFFS mock.
#include "test_helpers.h"

// ---------------------------------------------------------------- init ----

TEST(QuarkDBInit, MountsSpiffs) {
  SPIFFS.reset();
  QuarkDB db;
  EXPECT_TRUE(db.init(QUARKDB_SPIFFS_FILE_TYPE));
  EXPECT_EQ(SPIFFS.beginCalls(), 1);
}

TEST(QuarkDBInit, RejectsUnsupportedFileMode) {
  SPIFFS.reset();
  Serial.reset();
  QuarkDB db;
  EXPECT_FALSE(db.init(1));
  EXPECT_NE(Serial.output().find("File mode not supported"), std::string::npos);
}

#if defined(ESP32)
TEST(QuarkDBInit, FailsWhenSpiffsMountFailsOnEsp32) {
  SPIFFS.reset();
  SPIFFS.setBeginResult(false);
  QuarkDB db;
  EXPECT_FALSE(db.init(QUARKDB_SPIFFS_FILE_TYPE));
}
#endif

// --------------------------------------------------------- create list ----

TEST_F(QuarkDBTest, CreateListCreatesEmptyFile) {
  EXPECT_TRUE(db.createList("sensors"));
  EXPECT_TRUE(SPIFFS.exists(listPath("sensors").c_str()));
  EXPECT_EQ(SPIFFS.readRaw(listPath("sensors")), "");
  EXPECT_EQ(db.getRecordCount("sensors"), 0);
}

TEST_F(QuarkDBTest, CreateListTrimsWhitespace) {
  EXPECT_TRUE(db.createList("  sensors  "));
  EXPECT_TRUE(SPIFFS.exists(listPath("sensors").c_str()));
}

TEST_F(QuarkDBTest, CreateListAcceptsAlnumUnderscoreDash) {
  EXPECT_TRUE(db.createList("a_B-9"));
}

TEST_F(QuarkDBTest, CreateListAcceptsUpTo12Characters) {
  EXPECT_TRUE(db.createList("abcdefghijkl"));
  EXPECT_EQ(listPath("abcdefghijkl").size(), fs::MockFS::kMaxPathLen);
  EXPECT_TRUE(add("abcdefghijkl", R"({"x":1})"));
  EXPECT_EQ(update("abcdefghijkl", "{}", R"({"x":2})"), 1);
  EXPECT_EQ(db.deleteRecords("abcdefghijkl", "{}"), 1);
  EXPECT_TRUE(db.deleteList("abcdefghijkl"));
}

TEST_F(QuarkDBTest, CreateListRejectsNamesLongerThan12) {
  EXPECT_FALSE(db.createList("abcdefghijklm"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}

TEST_F(QuarkDBTest, CreateListRejectsDuplicate) {
  ASSERT_TRUE(db.createList("sensors"));
  EXPECT_FALSE(db.createList("sensors"));
  EXPECT_NE(Serial.output().find("List already exists"), std::string::npos);
}

TEST_F(QuarkDBTest, CreateListRejectsInvalidNames) {
  EXPECT_FALSE(db.createList(""));
  EXPECT_FALSE(db.createList("   "));
  EXPECT_FALSE(db.createList("has space"));
  EXPECT_FALSE(db.createList("../escape"));
  EXPECT_FALSE(db.createList("dot.name"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}

TEST_F(QuarkDBTest, CreateListFailsWhenFileCannotBeOpened) {
  SPIFFS.failOpen("w");
  EXPECT_FALSE(db.createList("sensors"));
}

// --------------------------------------------------------- delete list ----

TEST_F(QuarkDBTest, DeleteListRemovesFile) {
  seedPeople();
  EXPECT_TRUE(db.deleteList("people"));
  EXPECT_FALSE(SPIFFS.exists(listPath("people").c_str()));
  EXPECT_EQ(db.getRecordCount("people"), -1);
}

TEST_F(QuarkDBTest, DeleteListFailsForMissingOrInvalidList) {
  EXPECT_FALSE(db.deleteList("nope"));
  EXPECT_FALSE(db.deleteList("bad name"));
}

TEST_F(QuarkDBTest, DeleteListOnlyAffectsThatList) {
  ASSERT_TRUE(db.createList("one"));
  ASSERT_TRUE(db.createList("two"));
  ASSERT_TRUE(add("two", R"({"x":1})"));
  EXPECT_TRUE(db.deleteList("one"));
  EXPECT_EQ(db.getRecordCount("two"), 1);
}

// ------------------------------------------------------------- add -------

TEST_F(QuarkDBTest, AddRecordAppendsJsonLine) {
  ASSERT_TRUE(db.createList("people"));
  EXPECT_TRUE(add("people", R"({"name":"alice","age":30})"));
  EXPECT_TRUE(add("people", R"({"name":"bob","age":43})"));
  EXPECT_EQ(SPIFFS.readRaw(listPath("people")),
            "{\"name\":\"alice\",\"age\":30}\r\n{\"name\":\"bob\",\"age\":43}\r\n");
  EXPECT_EQ(db.getRecordCount("people"), 2);
}

TEST_F(QuarkDBTest, AddRecordFailsForMissingList) {
  EXPECT_FALSE(add("ghost", R"({"x":1})"));
  EXPECT_FALSE(SPIFFS.exists(listPath("ghost").c_str()));
}

TEST_F(QuarkDBTest, AddRecordFailsForInvalidListName) {
  EXPECT_FALSE(add("bad name", R"({"x":1})"));
}

TEST_F(QuarkDBTest, AddRecordFailsWhenAppendOpenFails) {
  ASSERT_TRUE(db.createList("people"));
  SPIFFS.failOpen("a");
  EXPECT_FALSE(add("people", R"({"x":1})"));
  EXPECT_EQ(db.getRecordCount("people"), 0);
}

TEST_F(QuarkDBTest, AddRecordRoundTripsNestedStructures) {
  ASSERT_TRUE(db.createList("nested"));
  ASSERT_TRUE(add("nested", R"({"a":{"b":{"c":[1,{"d":"e"}]}},"f":null,"g":-1.5})"));
  JsonDocument result;
  ASSERT_EQ(db.getRecords("nested", "{}", &result), 1);
  JsonObject rec = result[0];
  EXPECT_EQ(rec["a"]["b"]["c"][1]["d"], "e");
  EXPECT_TRUE(rec["f"].isNull());
  EXPECT_DOUBLE_EQ(rec["g"].as<double>(), -1.5);
}

// ------------------------------------------------------------ count ------

TEST_F(QuarkDBTest, CountReturnsMinusOneForMissingOrInvalidList) {
  EXPECT_EQ(db.getRecordCount("ghost"), -1);
  EXPECT_EQ(db.getRecordCount("bad name"), -1);
}

TEST_F(QuarkDBTest, CountIgnoresBlankLines) {
  SPIFFS.writeRaw(listPath("raw"), "{\"a\":1}\n\n{\"a\":2}\n\r\n\n");
  EXPECT_EQ(db.getRecordCount("raw"), 2);
}

TEST_F(QuarkDBTest, CountSkipsCorruptLinesLikeGetRecords) {
  SPIFFS.writeRaw(listPath("raw"), "{\"name\":\"ok\"}\r\n{broken\r\n");
  EXPECT_EQ(db.getRecordCount("raw"), 1);
}

TEST_F(QuarkDBTest, CountAbove255Records) {
  ASSERT_TRUE(db.createList("big"));
  for (int i = 0; i < 300; i++) ASSERT_TRUE(add("big", R"({"v":1})"));
  EXPECT_EQ(db.getRecordCount("big"), 300);
  JsonDocument result;
  EXPECT_EQ(db.getRecords("big", "{}", &result), 300);
  EXPECT_EQ(result.size(), 300u);
}

// ------------------------------------------------------------- read ------

TEST_F(QuarkDBTest, GetRecordsEmptyFilterReturnsAllInInsertionOrder) {
  seedPeople();
  int count = 0;
  EXPECT_EQ(findNames("people", "{}", &count), (Names{"alice", "bob", "carol", "dave", "erin"}));
  EXPECT_EQ(count, 5);
}

TEST_F(QuarkDBTest, GetRecordsOnEmptyListReturnsEmptyArray) {
  ASSERT_TRUE(db.createList("empty"));
  JsonDocument result;
  EXPECT_EQ(db.getRecords("empty", "{}", &result), 0);
  ASSERT_TRUE(result.is<JsonArray>());
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(QuarkDBTest, GetRecordsOnMissingOrInvalidListReturnsError) {
  JsonDocument result;
  EXPECT_EQ(db.getRecords("ghost", "{}", &result), -1);
  EXPECT_EQ(db.getRecords("bad name", "{}", &result), -1);
}

TEST_F(QuarkDBTest, GetRecordsRejectsInvalidFilterJson) {
  seedPeople();
  JsonDocument result;
  EXPECT_EQ(db.getRecords("people", "{not json", &result), -1);
}

TEST_F(QuarkDBTest, GetRecordsSkipsCorruptLines) {
  SPIFFS.writeRaw(listPath("raw"), "{\"name\":\"ok1\"}\n{broken\n{\"name\":\"ok2\"}\n");
  EXPECT_EQ(findNames("raw", "{}"), (Names{"ok1", "ok2"}));
}

TEST_F(QuarkDBTest, GetRecordsEqualityOnInt) {
  seedPeople();
  int count = 0;
  EXPECT_EQ(findNames("people", R"({"age":43})", &count), (Names{"bob"}));
  EXPECT_EQ(count, 1);
}

TEST_F(QuarkDBTest, GetRecordsEqualityOnFloat) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"temp":23.4})"), (Names{"alice"}));
}

TEST_F(QuarkDBTest, GetRecordsEqualityOnString) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"name":"carol"})"), (Names{"carol"}));
}

TEST_F(QuarkDBTest, GetRecordsEqualityOnBool) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"active":false})"), (Names{"bob", "dave"}));
}

TEST_F(QuarkDBTest, GetRecordsNoMatchReturnsZero) {
  seedPeople();
  int count = -5;
  EXPECT_TRUE(findNames("people", R"({"name":"zed"})", &count).empty());
  EXPECT_EQ(count, 0);
}

TEST_F(QuarkDBTest, GetRecordsTypeMismatchDoesNotMatch) {
  seedPeople();
  EXPECT_TRUE(findNames("people", R"({"age":"30"})").empty());
  EXPECT_TRUE(findNames("people", R"({"name":30})").empty());
}

TEST_F(QuarkDBTest, GetRecordsMultiKeySelectorRequiresAllKeys) {
  seedPeople();
  // first key fails for bob (DEL), last key matches; must not match
  EXPECT_TRUE(findNames("people", R"({"address.city":"BLR","age":43})").empty());
  // last key fails; must not match either
  EXPECT_TRUE(findNames("people", R"({"age":43,"address.city":"BLR"})").empty());
  EXPECT_EQ(findNames("people", R"({"age":30,"address.city":"BLR","active":true})"), (Names{"alice"}));
}

TEST_F(QuarkDBTest, GetRecordsInvalidDotPathDoesNotMatch) {
  seedPeople();
  EXPECT_TRUE(findNames("people", R"({"tags.x":"a"})").empty());      // non-numeric array index
  EXPECT_TRUE(findNames("people", R"({"name.first":"a"})").empty());  // path through a string
  EXPECT_TRUE(findNames("people", R"({"age.0.x":1})").empty());       // path through a number
}

TEST_F(QuarkDBTest, GetRecordsMissingFieldDoesNotMatch) {
  seedPeople();
  EXPECT_TRUE(findNames("people", R"({"salary":100})").empty());
}

TEST_F(QuarkDBTest, GetRecordsDotPathIntoNestedObject) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"address.city":"BLR"})"), (Names{"alice", "carol"}));
  EXPECT_EQ(findNames("people", R"({"address.pin":110001})"), (Names{"bob"}));
}

TEST_F(QuarkDBTest, GetRecordsDotPathIntoArrayIndex) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"tags.0":"a"})"), (Names{"alice", "erin"}));
  EXPECT_EQ(findNames("people", R"({"scores.1":20})"), (Names{"alice"}));
}

TEST_F(QuarkDBTest, GetRecordsComparisonOperators) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"age":{"$gt":36}})"), (Names{"bob", "carol"}));
  EXPECT_EQ(findNames("people", R"({"age":{"$gte":36}})"), (Names{"bob", "carol", "erin"}));
  EXPECT_EQ(findNames("people", R"({"age":{"$lt":30}})"), (Names{"dave"}));
  EXPECT_EQ(findNames("people", R"({"age":{"$lte":30}})"), (Names{"alice", "dave"}));
  EXPECT_EQ(findNames("people", R"({"temp":{"$gte":25.8}})"), (Names{"carol", "dave", "erin"}));
}

TEST_F(QuarkDBTest, GetRecordsRangeWithTwoOperatorsAppliesBoth) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"age":{"$gt":25,"$lt":40}})"), (Names{"alice", "erin"}));
  EXPECT_EQ(findNames("people", R"({"age":{"$lt":40,"$gt":25}})"), (Names{"alice", "erin"}));
  EXPECT_EQ(findNames("people", R"({"age":{"$gte":30,"$lte":36},"active":true})"), (Names{"alice", "erin"}));
}

TEST_F(QuarkDBTest, GetRecordsEqOperator) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"age":{"$eq":24}})"), (Names{"dave"}));
  EXPECT_EQ(findNames("people", R"({"name":{"$eq":"erin"}})"), (Names{"erin"}));
  EXPECT_EQ(findNames("people", R"({"active":{"$eq":true}})"), (Names{"alice", "carol", "erin"}));
}

TEST_F(QuarkDBTest, GetRecordsUnknownOperatorMatchesNothing) {
  seedPeople();
  EXPECT_TRUE(findNames("people", R"({"age":{"$ne":30}})").empty());
}

TEST_F(QuarkDBTest, GetRecordsOperatorAgainstNonNumberDoesNotMatch) {
  seedPeople();
  EXPECT_TRUE(findNames("people", R"({"name":{"$gt":1}})").empty());
}

TEST_F(QuarkDBTest, GetRecordsEleMatchOnScalarArray) {
  seedPeople();
  EXPECT_EQ(findNames("people", R"({"scores":{"$eleMatch":{"$gt":55}}})"), (Names{"carol", "erin"}));
  EXPECT_EQ(findNames("people", R"({"scores":{"$eleMatch":{"$eq":5}}})"), (Names{"bob"}));
  EXPECT_EQ(findNames("people", R"({"scores":{"$eleMatch":{"$lt":3}}})"), (Names{"dave"}));
  EXPECT_EQ(findNames("people", R"({"tags":{"$eleMatch":{"$eq":"b"}}})"), (Names{"alice", "bob"}));
}

TEST_F(QuarkDBTest, GetRecordsEleMatchOnEmptyArrayDoesNotMatch) {
  seedPeople();
  // carol has "tags":[]
  EXPECT_TRUE(findNames("people", R"({"tags":{"$eleMatch":{"$eq":"zzz"}}})").empty());
  EXPECT_EQ(findNames("people", R"({"tags":{"$eleMatch":{"$eq":"c"}}})"), (Names{"dave"}));
}

TEST_F(QuarkDBTest, GetRecordsEleMatchOnObjectArray) {
  ASSERT_TRUE(db.createList("orders"));
  ASSERT_TRUE(add("orders", R"({"name":"o1","items":[{"sku":"x","qty":1},{"sku":"y","qty":5}]})"));
  ASSERT_TRUE(add("orders", R"({"name":"o2","items":[{"sku":"z","qty":2}]})"));
  EXPECT_EQ(findNames("orders", R"({"items":{"$eleMatch":{"sku":"y"}}})"), (Names{"o1"}));
  EXPECT_EQ(findNames("orders", R"({"items":{"$eleMatch":{"qty":2}}})"), (Names{"o2"}));
}

TEST_F(QuarkDBTest, GetRecordsLimitRowsCapsResultButReportsTotal) {
  seedPeople();
  JsonDocument result;
  int total = db.getRecords("people", "{}", &result, 2);
  EXPECT_EQ(total, 5);  // return value counts every match, not just returned rows
  EXPECT_EQ(names(result), (Names{"alice", "bob"}));
}

TEST_F(QuarkDBTest, GetRecordsLimitAbove255IsNotTruncated) {
  ASSERT_TRUE(db.createList("big"));
  for (int i = 0; i < 300; i++) ASSERT_TRUE(add("big", R"({"v":1})"));
  JsonDocument all;
  EXPECT_EQ(db.getRecords("big", "{}", &all, 300), 300);
  EXPECT_EQ(all.size(), 300u);
  JsonDocument some;
  EXPECT_EQ(db.getRecords("big", "{}", &some, 256), 300);
  EXPECT_EQ(some.size(), 256u);
}

TEST_F(QuarkDBTest, GetRecordsLimitZeroReturnsEmptyArray) {
  seedPeople();
  JsonDocument result;
  db.getRecords("people", "{}", &result, 0);
  EXPECT_EQ(result.size(), 0u);
}

TEST_F(QuarkDBTest, GetRecordsFailsWhenReadOpenFails) {
  seedPeople();
  SPIFFS.failOpen("r");
  JsonDocument result;
  EXPECT_EQ(db.getRecords("people", "{}", &result), -1);
  EXPECT_EQ(db.getRecordCount("people"), -1);
}

// ----------------------------------------------------------- update ------

TEST_F(QuarkDBTest, UpdateReplacesMatchingRecords) {
  seedPeople();
  EXPECT_EQ(update("people", R"({"age":30})", R"({"name":"alice","age":31})"), 1);
  JsonDocument result;
  ASSERT_EQ(db.getRecords("people", R"({"name":"alice"})", &result), 1);
  EXPECT_EQ(result[0]["age"], 31);
  // update is a full replace: fields not in the update object are gone
  EXPECT_TRUE(result[0]["temp"].isNull());
  EXPECT_EQ(db.getRecordCount("people"), 5);
}

TEST_F(QuarkDBTest, UpdatePreservesOrderOfRecords) {
  seedPeople();
  EXPECT_EQ(update("people", R"({"name":"carol"})", R"({"name":"CAROL"})"), 1);
  EXPECT_EQ(findNames("people", "{}"), (Names{"alice", "bob", "CAROL", "dave", "erin"}));
}

TEST_F(QuarkDBTest, UpdateMultipleMatches) {
  seedPeople();
  EXPECT_EQ(update("people", R"({"address.city":"DEL"})", R"({"name":"moved"})"), 2);
  EXPECT_EQ(findNames("people", "{}"), (Names{"alice", "moved", "carol", "dave", "moved"}));
}

TEST_F(QuarkDBTest, UpdateWithOperatorSelector) {
  seedPeople();
  EXPECT_EQ(update("people", R"({"age":{"$gte":43}})", R"({"name":"senior"})"), 2);
  EXPECT_EQ(findNames("people", R"({"name":"senior"})").size(), 2u);
}

TEST_F(QuarkDBTest, UpdateNoMatchLeavesDataIntact) {
  seedPeople();
  std::string before = SPIFFS.readRaw(listPath("people"));
  EXPECT_EQ(update("people", R"({"age":99})", R"({"name":"x"})"), 0);
  EXPECT_EQ(SPIFFS.readRaw(listPath("people")), before);  // no rewrite when nothing matched
  EXPECT_FALSE(SPIFFS.exists(tmpPath("people").c_str()));
}

TEST_F(QuarkDBTest, UpdateErrors) {
  seedPeople();
  EXPECT_EQ(update("people", "{bad", R"({"x":1})"), -1);
  EXPECT_EQ(update("ghost", "{}", R"({"x":1})"), -1);
  EXPECT_EQ(update("bad name", "{}", R"({"x":1})"), -1);
}

TEST_F(QuarkDBTest, UpdateFailsWhenRewriteOpenFails) {
  seedPeople();
  std::string before = SPIFFS.readRaw(listPath("people"));
  SPIFFS.failOpen("w");
  EXPECT_EQ(update("people", "{}", R"({"x":1})"), -1);
  EXPECT_EQ(SPIFFS.readRaw(listPath("people")), before);
}

TEST_F(QuarkDBTest, UpdateThenAddKeepsFileWellFormed) {
  seedPeople();
  ASSERT_EQ(update("people", R"({"name":"alice"})", R"({"name":"ann"})"), 1);
  ASSERT_TRUE(add("people", R"({"name":"frank"})"));
  EXPECT_EQ(findNames("people", "{}"), (Names{"ann", "bob", "carol", "dave", "erin", "frank"}));
  EXPECT_EQ(db.getRecordCount("people"), 6);
}

// ----------------------------------------------------------- delete ------

TEST_F(QuarkDBTest, DeleteRecordsRemovesMatches) {
  seedPeople();
  EXPECT_EQ(db.deleteRecords("people", R"({"active":true})"), 3);
  EXPECT_EQ(findNames("people", "{}"), (Names{"bob", "dave"}));
  EXPECT_EQ(db.getRecordCount("people"), 2);
}

TEST_F(QuarkDBTest, DeleteRecordsEmptyFilterRemovesAll) {
  seedPeople();
  EXPECT_EQ(db.deleteRecords("people", "{}"), 5);
  EXPECT_EQ(db.getRecordCount("people"), 0);
  EXPECT_EQ(SPIFFS.readRaw(listPath("people")), "");
  EXPECT_TRUE(findNames("people", "{}").empty());
  ASSERT_TRUE(add("people", R"({"name":"new"})"));
  EXPECT_EQ(db.getRecordCount("people"), 1);
  EXPECT_TRUE(SPIFFS.exists(listPath("people").c_str()));
}

TEST_F(QuarkDBTest, DeleteRecordsErrorsReturnMinusOne) {
  seedPeople();
  EXPECT_EQ(db.deleteRecords("people", "{bad"), -1);
  EXPECT_EQ(db.deleteRecords("ghost", "{}"), -1);
  EXPECT_EQ(db.deleteRecords("bad name", "{}"), -1);
  EXPECT_EQ(db.getRecordCount("people"), 5);
}

TEST_F(QuarkDBTest, DeleteRecordsNoMatch) {
  seedPeople();
  EXPECT_EQ(db.deleteRecords("people", R"({"age":1})"), 0);
  EXPECT_EQ(db.getRecordCount("people"), 5);
}

TEST_F(QuarkDBTest, DeleteRecordsWithOperatorSelector) {
  seedPeople();
  EXPECT_EQ(db.deleteRecords("people", R"({"age":{"$lt":35}})"), 2);
  EXPECT_EQ(findNames("people", "{}"), (Names{"bob", "carol", "erin"}));
}

TEST_F(QuarkDBTest, DeleteRecordsFailsWhenRewriteOpenFails) {
  seedPeople();
  SPIFFS.failOpen("w");
  EXPECT_EQ(db.deleteRecords("people", "{}"), -1);
  EXPECT_EQ(db.getRecordCount("people"), 5);
}

TEST_F(QuarkDBTest, DeleteThenAddKeepsFileWellFormed) {
  seedPeople();
  ASSERT_EQ(db.deleteRecords("people", R"({"name":"carol"})"), 1);
  ASSERT_TRUE(add("people", R"({"name":"frank"})"));
  EXPECT_EQ(findNames("people", "{}"), (Names{"alice", "bob", "dave", "erin", "frank"}));
  EXPECT_EQ(db.getRecordCount("people"), 5);
}

// ------------------------------------------------------- end to end ------

TEST_F(QuarkDBTest, FullLifecycleMatchesBasicApiExample) {
  ASSERT_TRUE(db.createList("testList"));
  const char* names[] = {"test-0", "test-1", "test-2", "test-3", "test-4"};
  int ages[] = {30, 43, 46, 24, 36};
  for (int i = 0; i < 5; i++) {
    JsonDocument doc;
    doc["name"] = names[i];
    doc["age"] = ages[i];
    ASSERT_TRUE(db.addRecord("testList", doc.as<JsonObject>()));
  }
  EXPECT_EQ(db.getRecordCount("testList"), 5);

  JsonDocument upd;
  upd["name"] = "test-0";
  upd["age"] = 33;
  EXPECT_EQ(db.updateRecords("testList", R"({"age":30})", upd.as<JsonObject>()), 1);

  JsonDocument r1;
  EXPECT_EQ(db.getRecords("testList", R"({"age":33})", &r1), 1);
  EXPECT_EQ(r1[0]["name"], "test-0");

  EXPECT_EQ(db.deleteRecords("testList", R"({"age":33})"), 1);
  JsonDocument r2;
  EXPECT_EQ(db.getRecords("testList", R"({"age":33})", &r2), 0);
  EXPECT_EQ(db.getRecordCount("testList"), 4);

  EXPECT_TRUE(db.deleteList("testList"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}

TEST_F(QuarkDBTest, ListsAreIsolated) {
  ASSERT_TRUE(db.createList("a"));
  ASSERT_TRUE(db.createList("b"));
  ASSERT_TRUE(add("a", R"({"name":"in-a"})"));
  ASSERT_TRUE(add("b", R"({"name":"in-b"})"));
  EXPECT_EQ(db.deleteRecords("a", "{}"), 1);
  EXPECT_EQ(findNames("b", "{}"), (Names{"in-b"}));
}
