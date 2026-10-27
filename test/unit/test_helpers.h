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

inline std::string newPath(const std::string& listName) {
  return "/__quarkdb__/" + listName + ".new";
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
