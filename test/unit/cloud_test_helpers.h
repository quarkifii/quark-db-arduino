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

// Fixture and helpers for the cloud backup tests (run with ./run_tests.sh --cloud).
// The DB talks to a mocked PubSubClient through QuarkMqttTransport; tests
// read what the device published and play the cloud by delivering messages
// on the subscribe topic.
#ifndef QUARKDB_CLOUD_TEST_HELPERS_H
#define QUARKDB_CLOUD_TEST_HELPERS_H

#include "test_helpers.h"

#include <PubSubClient.h>
#include <QuarkMqttTransport.h>
#include <utils/db_utils.h>

#include <map>
#include <sstream>
#include <string>
#include <vector>

inline std::string syncPath(const std::string& listName) {
  return "/__quarkdb__/" + listName + ".sync";
}

inline std::string oplogPath(const std::string& listName) {
  return "/__quarkdb__/" + listName + ".oplog";
}

inline std::string restorePath(const std::string& listName) {
  return "/__quarkdb__/" + listName + ".rst";
}

// Splits a JSONL file into its non-empty lines
inline std::vector<std::string> jsonLines(const std::string& content) {
  std::vector<std::string> lines;
  std::stringstream ss(content);
  std::string line;
  while (std::getline(ss, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (!line.empty()) lines.push_back(line);
  }
  return lines;
}

class CloudSyncTest : public QuarkDBTest {
 protected:
  static constexpr const char* kOut = "devices/esp-1/quarkdb/out";
  static constexpr const char* kIn = "devices/esp-1/quarkdb/in";

  void SetUp() override {
    QuarkDBTest::SetUp();
    mqtt.reset();
    mqtt.setBufferSize(4096);
    mqtt.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
      lastHandled = db.handleMessage(topic, payload, length);
    });
    config = QuarkSyncConfig();
    config.publishTopic = kOut;
    config.subscribeTopic = kIn;
  }

  // enableSync with the fixture config (after any tweaks to `config`)
  bool enableSync() { return db.enableSync(&transport, config); }

  // Brings the broker connection up and runs the reconnect hook
  int connectAndSync() {
    EXPECT_TRUE(mqtt.connect("esp-1"));
    return db.syncCloud();
  }

  // ---- what the device published ----
  size_t publishedCount() const { return mqtt.published().size(); }
  const std::string& payload(size_t i) { return mqtt.published().at(i).payload; }
  // Parsed copy of the i-th published message; fails the test on bad JSON
  JsonDocument message(size_t i) {
    JsonDocument doc;
    if (i >= publishedCount()) {
      ADD_FAILURE() << "no published message #" << i << " (only " << publishedCount() << ")";
      return doc;
    }
    if (deserializeJson(doc, payload(i))) ADD_FAILURE() << "published message is not JSON: " << payload(i);
    return doc;
  }
  std::string type(size_t i) { return message(i)["type"].as<std::string>(); }
  std::vector<std::string> types() {
    std::vector<std::string> out;
    for (size_t i = 0; i < publishedCount(); i++) out.push_back(type(i));
    return out;
  }
  unsigned long mid(size_t i) { return message(i)["mid"].as<unsigned long>(); }
  void clearPublished() {
    noteMids();
    mqtt.published().clear();
  }

  // ---- playing the cloud ----
  // A real cloud answers the query/restore it received: remember the mid of
  // the latest one per list so the helpers below can echo it
  void noteMids() {
    for (auto& m : mqtt.published()) {
      JsonDocument doc;
      if (deserializeJson(doc, m.payload)) continue;
      std::string type = doc["type"].as<std::string>();
      std::string list = doc["list"].as<std::string>();
      if (type == "query") queryMids[list] = doc["mid"].as<unsigned long>();
      if (type == "restore") restoreMids[list] = doc["mid"].as<unsigned long>();
    }
  }
  // Delivers a message on the subscribe topic (through the PubSubClient callback)
  bool cloudSends(const std::string& json) {
    noteMids();
    lastHandled = false;
    mqtt.deliver(kIn, json);
    return lastHandled;
  }
  // Answer to the latest query for the list (mid echoed); `extra` adds raw fields
  std::string statusJson(const std::string& list, const std::string& inst, long seq, const std::string& extra = "") {
    noteMids();
    return "{\"type\":\"status\",\"list\":\"" + list + "\",\"inst\":\"" + inst + "\",\"seq\":" + std::to_string(seq) +
           ",\"mid\":" + std::to_string(queryMids[list]) + extra + "}";
  }
  static std::string ackJson(const std::string& list, unsigned long mid, const std::string& extra = "") {
    return "{\"type\":\"ack\",\"list\":\"" + list + "\",\"mid\":" + std::to_string(mid) + extra + "}";
  }
  // CRC-32 over compact documents each followed by "\n", as the protocol defines it
  static unsigned long crcOfDocs(const std::vector<std::string>& docs, unsigned long crc = 0) {
    for (auto& d : docs) {
      JsonDocument doc;
      deserializeJson(doc, d);
      std::string compact;
      serializeJson(doc, compact);
      compact += "\n";
      crc = DBUtils::crc32(crc, compact.c_str(), compact.size());
    }
    return crc;
  }
  // Chunk answering the latest restore request for the list (mid echoed).
  // The checksum of everything sent since chunk 0 is added to the last
  // chunk; crcOverride/countOverride (>= 0) replace it for negative tests,
  // withChecksum=false omits it.
  std::string snapshotJson(const std::string& list, const std::string& inst, long seq, long index, bool last,
                           const std::string& docsArray, long long crcOverride = -1, long countOverride = -1,
                           bool withChecksum = true) {
    noteMids();
    if (index == 0) {
      restoreCrc[list] = 0;
      restoreCount[list] = 0;
    }
    JsonDocument arr;
    deserializeJson(arr, docsArray);
    for (JsonVariant v : arr.as<JsonArray>()) {
      if (!v.is<JsonObject>()) continue;
      std::string compact;
      serializeJson(v, compact);
      compact += "\n";
      restoreCrc[list] = DBUtils::crc32(restoreCrc[list], compact.c_str(), compact.size());
      restoreCount[list]++;
    }
    std::string json = "{\"type\":\"snapshot\",\"list\":\"" + list + "\",\"inst\":\"" + inst + "\",\"seq\":" + std::to_string(seq) +
                       ",\"mid\":" + std::to_string(restoreMids[list]) + ",\"i\":" + std::to_string(index) +
                       ",\"last\":" + (last ? "true" : "false") + ",\"docs\":" + docsArray;
    if (last && withChecksum) {
      json += ",\"crc\":" + std::to_string(crcOverride >= 0 ? (unsigned long)crcOverride : restoreCrc[list]);
      json += ",\"count\":" + std::to_string(countOverride >= 0 ? countOverride : restoreCount[list]);
    }
    return json + "}";
  }

  // ---- sync state of a list ----
  JsonDocument info(const String& list) {
    JsonDocument doc;
    if (!db.getSyncInfo(list, &doc)) ADD_FAILURE() << "no sync info for " << list.c_str();
    return doc;
  }
  std::string inst(const String& list) { return info(list)["inst"].as<std::string>(); }
  long seq(const String& list) { return info(list)["seq"].as<long>(); }
  long cloudSeq(const String& list) { return info(list)["cloudSeq"].as<long>(); }
  std::string state(const String& list) { return info(list)["state"].as<std::string>(); }
  int pending(const String& list) { return info(list)["pending"].as<int>(); }
  std::vector<std::string> oplogLines(const std::string& list) { return jsonLines(SPIFFS.readRaw(oplogPath(list))); }

  // Typical start: a synced list with three records, connection still down
  void seedSynced(const String& list = "cfg") {
    ASSERT_TRUE(db.createList(list));
    ASSERT_TRUE(enableSync());
    ASSERT_TRUE(db.enableListSync(list));
    ASSERT_TRUE(add(list, R"({"name":"alice","age":30})"));
    ASSERT_TRUE(add(list, R"({"name":"bob","age":43})"));
    ASSERT_TRUE(add(list, R"({"name":"carol","age":46})"));
  }

  // Simulates a reboot: RAM state is gone, flash is kept
  void reboot() {
    SPIFFS.powerOn();
    ASSERT_TRUE(db.init(QUARKDB_SPIFFS_FILE_TYPE));
    ASSERT_TRUE(enableSync());
  }

  PubSubClient mqtt;
  QuarkMqttTransport transport{mqtt};
  QuarkSyncConfig config;
  bool lastHandled = false;
  std::map<std::string, unsigned long> queryMids, restoreMids, restoreCrc;
  std::map<std::string, long> restoreCount;
};

#endif
