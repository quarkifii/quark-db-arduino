// Shared fixture and helpers for QuarkDB host unit tests.
#ifndef QUARKDB_TEST_HELPERS_H
#define QUARKDB_TEST_HELPERS_H

#include <gtest/gtest.h>

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include <QuarkDB.h>

#include <string>

inline std::string listPath(const std::string& listName) {
  return "/__quarkdb__/" + listName + ".jsonl";
}

inline std::string tmpPath(const std::string& listName) {
  return "/__quarkdb__/" + listName + ".tmp";
}

// Resets the mocked SPIFFS and Serial and gives every test an initialised DB.
class QuarkDBTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SPIFFS.reset();
    Serial.reset();
    ASSERT_TRUE(db.init(QUARKDB_SPIFFS_FILE_TYPE));
  }

  bool add(const String& list, const char* json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
      ADD_FAILURE() << "test json does not parse: " << json;
      return false;
    }
    return db.addRecord(list, doc.as<JsonObject>());
  }

  int update(const String& list, const String& selector, const char* json) {
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
      ADD_FAILURE() << "test json does not parse: " << json;
      return -2;
    }
    return db.updateRecords(list, selector, doc.as<JsonObject>());
  }

  // Seeds a list with five people, matching the shape used in the examples.
  void seedPeople(const String& list = "people") {
    ASSERT_TRUE(db.createList(list));
    ASSERT_TRUE(add(list, R"({"name":"alice","age":30,"temp":23.4,"active":true,"address":{"city":"BLR","pin":560001},"tags":["a","b"],"scores":[10,20,30]})"));
    ASSERT_TRUE(add(list, R"({"name":"bob","age":43,"temp":24.8,"active":false,"address":{"city":"DEL","pin":110001},"tags":["b"],"scores":[5]})"));
    ASSERT_TRUE(add(list, R"({"name":"carol","age":46,"temp":25.8,"active":true,"address":{"city":"BLR","pin":560002},"tags":[],"scores":[50,60]})"));
    ASSERT_TRUE(add(list, R"({"name":"dave","age":24,"temp":26.1,"active":false,"address":{"city":"MUM","pin":400001},"tags":["c"],"scores":[1,2]})"));
    ASSERT_TRUE(add(list, R"({"name":"erin","age":36,"temp":25.9,"active":true,"address":{"city":"DEL","pin":110002},"tags":["a"],"scores":[99]})"));
  }

  // Returns the "name" field of each record in a getRecords() result.
  static std::vector<std::string> names(JsonDocument& result) {
    std::vector<std::string> out;
    for (JsonVariant v : result.as<JsonArray>()) out.push_back(v["name"].as<std::string>());
    return out;
  }

  std::vector<std::string> findNames(const String& list, const String& selector, int* count = nullptr) {
    JsonDocument result;
    int n = db.getRecords(list, selector, &result);
    if (count) *count = n;
    return names(result);
  }

  QuarkDB db;
};

using Names = std::vector<std::string>;

#endif
