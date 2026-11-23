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

// Oplog tests: enabling sync per list, what gets logged for each kind of
// write (API and CLI), sequence numbers, persistence across reboots and the
// size-capped rollover. No broker involved.
#include "cloud_test_helpers.h"

class CloudOplogTest : public CloudSyncTest {
 protected:
  std::string cli(const std::string& line) {
    Serial.reset();
    Serial.feed(line + "\n");
    db.processSerialCommand();
    return Serial.output();
  }
};

// ------------------------------------------------------ enable/disable ----

TEST_F(CloudOplogTest, EnableSyncValidatesArguments) {
  EXPECT_FALSE(db.isSyncEnabled());
  EXPECT_FALSE(db.enableSync(nullptr, config));
  QuarkSyncConfig noTopics;
  EXPECT_FALSE(db.enableSync(&transport, noTopics));
  QuarkSyncConfig tiny = config;
  tiny.maxPayloadBytes = 10;
  EXPECT_FALSE(db.enableSync(&transport, tiny));
  EXPECT_FALSE(db.isSyncEnabled());
  EXPECT_TRUE(enableSync());
  EXPECT_TRUE(db.isSyncEnabled());
  db.disableSync();
  EXPECT_FALSE(db.isSyncEnabled());
}

TEST_F(CloudOplogTest, EnableListSyncRequiresExistingValidList) {
  EXPECT_FALSE(db.enableListSync("ghost"));
  EXPECT_FALSE(db.enableListSync("bad name"));
  EXPECT_FALSE(db.enableListSync(""));
  EXPECT_FALSE(SPIFFS.exists(syncPath("ghost").c_str()));
  ASSERT_TRUE(db.createList("cfg"));
  EXPECT_TRUE(db.enableListSync("  cfg  "));
  EXPECT_TRUE(db.isListSyncEnabled("cfg"));
}

TEST_F(CloudOplogTest, SyncFilesFitSpiffsPathLimitForLongestListName) {
  ASSERT_TRUE(db.createList("abcdefghijkl"));
  ASSERT_TRUE(db.enableListSync("abcdefghijkl"));
  EXPECT_EQ(oplogPath("abcdefghijkl").size(), fs::MockFS::kMaxPathLen);
  ASSERT_TRUE(add("abcdefghijkl", R"({"x":1})"));
  EXPECT_EQ(oplogLines("abcdefghijkl").size(), 1u);
  EXPECT_EQ(seq("abcdefghijkl"), 1);
  EXPECT_TRUE(db.disableListSync("abcdefghijkl"));
}

TEST_F(CloudOplogTest, EnableListSyncWorksWithoutTransport) {
  // Changes are logged before enableSync() is ever called (e.g. WiFi not up yet)
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"k":1})"));
  EXPECT_EQ(oplogLines("cfg").size(), 1u);
  EXPECT_EQ(seq("cfg"), 1);
}

TEST_F(CloudOplogTest, EnableListSyncCreatesMetadataWithInstanceId) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(SPIFFS.exists(syncPath("cfg").c_str()));
  JsonDocument meta;
  ASSERT_FALSE(deserializeJson(meta, SPIFFS.readRaw(syncPath("cfg"))));
  std::string id = meta["inst"].as<std::string>();
  EXPECT_FALSE(id.empty());
  EXPECT_EQ(meta["seq"].as<long>(), 0);
  EXPECT_EQ(meta["restore"].as<int>(), (int)QUARK_RESTORE_INHERIT);
  EXPECT_EQ(inst("cfg"), id);
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_EQ(cloudSeq("cfg"), -1);
  EXPECT_EQ(pending("cfg"), 0);
}

TEST_F(CloudOplogTest, InstanceIdsAreUnique) {
  ASSERT_TRUE(db.createList("a"));
  ASSERT_TRUE(db.createList("b"));
  ASSERT_TRUE(db.enableListSync("a"));
  ASSERT_TRUE(db.enableListSync("b"));
  EXPECT_NE(inst("a"), inst("b"));
  std::string first = inst("a");
  ASSERT_TRUE(db.disableListSync("a"));
  ASSERT_TRUE(db.enableListSync("a"));
  EXPECT_NE(inst("a"), first);
}

TEST_F(CloudOplogTest, EnableListSyncAgainKeepsInstanceAndUpdatesOptions) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  std::string id = inst("cfg");
  ASSERT_TRUE(add("cfg", R"({"k":1})"));
  QuarkListSyncOptions options;
  options.restore = QUARK_RESTORE_ON_MISMATCH;
  ASSERT_TRUE(db.enableListSync("cfg", options));
  EXPECT_EQ(inst("cfg"), id);
  EXPECT_EQ(seq("cfg"), 1);
  EXPECT_EQ(oplogLines("cfg").size(), 1u);
  EXPECT_EQ(info("cfg")["restore"].as<int>(), (int)QUARK_RESTORE_ON_MISMATCH);
}

TEST_F(CloudOplogTest, DisableListSyncRemovesFilesAndStopsLogging) {
  seedSynced("cfg");
  ASSERT_TRUE(SPIFFS.exists(oplogPath("cfg").c_str()));
  EXPECT_TRUE(db.disableListSync("cfg"));
  EXPECT_FALSE(SPIFFS.exists(syncPath("cfg").c_str()));
  EXPECT_FALSE(SPIFFS.exists(oplogPath("cfg").c_str()));
  EXPECT_FALSE(db.isListSyncEnabled("cfg"));
  EXPECT_FALSE(db.disableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  EXPECT_FALSE(SPIFFS.exists(oplogPath("cfg").c_str()));
  EXPECT_EQ(db.getRecordCount("cfg"), 4);  // the list itself is untouched
  JsonDocument none;
  EXPECT_FALSE(db.getSyncInfo("cfg", &none));
}

TEST_F(CloudOplogTest, UnsyncedListsAreNotLogged) {
  ASSERT_TRUE(db.createList("plain"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(add("plain", R"({"k":1})"));
  EXPECT_EQ(update("plain", "{}", R"({"k":2})"), 1);
  EXPECT_EQ(db.deleteRecords("plain", "{}"), 1);
  EXPECT_FALSE(SPIFFS.exists(oplogPath("plain").c_str()));
  EXPECT_FALSE(SPIFFS.exists(syncPath("plain").c_str()));
  EXPECT_FALSE(db.isListSyncEnabled("plain"));
}

TEST_F(CloudOplogTest, TooManySyncedListsIsRejected) {
  for (int i = 0; i < __QUARKDB_MAX_SYNC_LISTS__; i++) {
    String name = "l" + String(i);
    ASSERT_TRUE(db.createList(name));
    ASSERT_TRUE(db.enableListSync(name)) << name.c_str();
  }
  ASSERT_TRUE(db.createList("extra"));
  EXPECT_FALSE(db.enableListSync("extra"));
  EXPECT_FALSE(SPIFFS.exists(syncPath("extra").c_str()));
  ASSERT_TRUE(db.disableListSync("l0"));
  EXPECT_TRUE(db.enableListSync("extra"));
}

// --------------------------------------------------------- entries -----

TEST_F(CloudOplogTest, EveryWriteAppendsAnEntryWithSequenceAndType) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"name":"alice","age":30})"));
  ASSERT_TRUE(add("cfg", R"({"name":"bob","age":43})"));
  EXPECT_EQ(update("cfg", R"({"age":30})", R"({"name":"alice","age":31})"), 1);
  EXPECT_EQ(db.deleteRecords("cfg", R"({"name":"bob"})"), 1);
  auto lines = oplogLines("cfg");
  ASSERT_EQ(lines.size(), 4u);
  EXPECT_EQ(lines[0], R"({"seq":1,"op":"add","doc":{"name":"alice","age":30}})");
  EXPECT_EQ(lines[1], R"({"seq":2,"op":"add","doc":{"name":"bob","age":43}})");
  EXPECT_EQ(lines[2], R"({"seq":3,"op":"update","sel":{"age":30},"doc":{"name":"alice","age":31},"n":1})");
  EXPECT_EQ(lines[3], R"({"seq":4,"op":"delete","sel":{"name":"bob"},"n":1})");
  EXPECT_EQ(seq("cfg"), 4);
  EXPECT_EQ(pending("cfg"), 4);
  EXPECT_EQ(db.getRecordCount("cfg"), 1);
}

TEST_F(CloudOplogTest, SelectorIsStoredCanonically) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"age":30})"));
  EXPECT_EQ(db.deleteRecords("cfg", "{ \"age\" :\n 30 }"), 1);
  EXPECT_EQ(oplogLines("cfg")[1], R"({"seq":2,"op":"delete","sel":{"age":30},"n":1})");
}

TEST_F(CloudOplogTest, NoOpWritesAreNotLogged) {
  seedSynced("cfg");
  size_t before = oplogLines("cfg").size();
  EXPECT_EQ(update("cfg", R"({"age":99})", R"({"x":1})"), 0);
  EXPECT_EQ(db.deleteRecords("cfg", R"({"age":99})"), 0);
  EXPECT_EQ(update("cfg", "{bad", R"({"x":1})"), -1);
  SPIFFS.failOpen("a");
  EXPECT_FALSE(add("cfg", R"({"x":1})"));
  SPIFFS.failOpen("a", false);
  EXPECT_EQ(oplogLines("cfg").size(), before);
  EXPECT_EQ(seq("cfg"), (long)before);
}

TEST_F(CloudOplogTest, MultiMatchWritesRecordTheCount) {
  seedSynced("cfg");
  EXPECT_EQ(update("cfg", R"({"age":{"$gt":40}})", R"({"name":"senior"})"), 2);
  EXPECT_EQ(db.deleteRecords("cfg", "{}"), 3);
  auto lines = oplogLines("cfg");
  ASSERT_EQ(lines.size(), 5u);
  EXPECT_EQ(lines[3], R"({"seq":4,"op":"update","sel":{"age":{"$gt":40}},"doc":{"name":"senior"},"n":2})");
  EXPECT_EQ(lines[4], R"({"seq":5,"op":"delete","sel":{},"n":3})");
}

TEST_F(CloudOplogTest, CliWritesAreLoggedToo) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  cli("QUARKDB>db.cfg.add({\"name\":\"alice\",\"age\":30})");
  cli("QUARKDB>db.cfg.update({ \"filter\" : {\"age\" : 30} , \"updateObj\" : {\"age\" : 31 , \"name\" : \"ann\"} })");
  cli("QUARKDB>db.cfg.delete({\"age\":31})");
  auto lines = oplogLines("cfg");
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(lines[0], R"({"seq":1,"op":"add","doc":{"name":"alice","age":30}})");
  EXPECT_EQ(lines[1], R"({"seq":2,"op":"update","sel":{"age":30},"doc":{"age":31,"name":"ann"},"n":1})");
  EXPECT_EQ(lines[2], R"({"seq":3,"op":"delete","sel":{"age":31},"n":1})");
}

TEST_F(CloudOplogTest, ListsHaveIndependentLogs) {
  ASSERT_TRUE(db.createList("a"));
  ASSERT_TRUE(db.createList("b"));
  ASSERT_TRUE(db.enableListSync("a"));
  ASSERT_TRUE(db.enableListSync("b"));
  ASSERT_TRUE(add("a", R"({"x":1})"));
  ASSERT_TRUE(add("a", R"({"x":2})"));
  ASSERT_TRUE(add("b", R"({"y":1})"));
  EXPECT_EQ(seq("a"), 2);
  EXPECT_EQ(seq("b"), 1);
  EXPECT_EQ(oplogLines("b")[0], R"({"seq":1,"op":"add","doc":{"y":1}})");
}

TEST_F(CloudOplogTest, MetadataFilesAreNotListsForShowLists) {
  seedSynced("cfg");
  std::string out = cli("QUARKDB>show lists");
  EXPECT_NE(out.find("Lists found are..\ncfg"), std::string::npos) << out;
  EXPECT_EQ(out.find("cfg\ncfg"), std::string::npos) << out;
}

// ------------------------------------------------------ persistence ----

TEST_F(CloudOplogTest, SyncStateSurvivesReboot) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  reboot();
  // No enableListSync() call after the reboot: the metadata on flash is enough
  EXPECT_TRUE(db.isListSyncEnabled("cfg"));
  EXPECT_EQ(inst("cfg"), id);
  EXPECT_EQ(seq("cfg"), 3);
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  EXPECT_EQ(seq("cfg"), 4);
  EXPECT_EQ(oplogLines("cfg").size(), 4u);
  EXPECT_EQ(oplogLines("cfg")[3], R"({"seq":4,"op":"add","doc":{"name":"dave"}})");
}

TEST_F(CloudOplogTest, EnableListSyncAfterRebootIsIdempotent) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  reboot();
  ASSERT_TRUE(db.enableListSync("cfg"));
  EXPECT_EQ(inst("cfg"), id);
  EXPECT_EQ(seq("cfg"), 3);
  EXPECT_EQ(oplogLines("cfg").size(), 3u);
}

TEST_F(CloudOplogTest, SequenceIsReconciledFromOplogAfterPowerLossBeforeMetadataWrite) {
  seedSynced("cfg");
  // Flash state after a power cut between the oplog append and the metadata rewrite
  SPIFFS.writeRaw(syncPath("cfg"), "{\"inst\":\"" + inst("cfg") + "\",\"seq\":2,\"restore\":-1}");
  reboot();
  EXPECT_EQ(seq("cfg"), 3);
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  EXPECT_EQ(oplogLines("cfg")[3], R"({"seq":4,"op":"add","doc":{"name":"dave"}})");
}

TEST_F(CloudOplogTest, MetadataSequenceIsRefreshedPeriodicallyAndReconciledFromOplog) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  auto metaSeq = [&] {
    JsonDocument meta;
    deserializeJson(meta, SPIFFS.readRaw(syncPath("cfg")));
    return meta["seq"].as<long>();
  };
  for (int i = 1; i < __QUARKDB_SYNC_META_SAVE_EVERY__; i++) ASSERT_TRUE(add("cfg", R"({"v":1})"));
  EXPECT_EQ(metaSeq(), 0);  // no metadata rewrite per change
  EXPECT_EQ(seq("cfg"), __QUARKDB_SYNC_META_SAVE_EVERY__ - 1);
  reboot();
  EXPECT_EQ(seq("cfg"), __QUARKDB_SYNC_META_SAVE_EVERY__ - 1);  // reconciled from the oplog
  ASSERT_TRUE(add("cfg", R"({"v":1})"));
  EXPECT_EQ(metaSeq(), __QUARKDB_SYNC_META_SAVE_EVERY__);
  ASSERT_TRUE(add("cfg", R"({"v":1})"));
  EXPECT_EQ(oplogLines("cfg").back(), "{\"seq\":" + std::to_string(__QUARKDB_SYNC_META_SAVE_EVERY__ + 1) + ",\"op\":\"add\",\"doc\":{\"v\":1}}");
}

TEST_F(CloudOplogTest, SkippedEntryPersistsItsSequenceNumber) {
  config.maxOplogBytes = 100;
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"v":1})"));
  std::string big = "{\"blob\":\"" + std::string(120, 'x') + "\"}";
  ASSERT_TRUE(add("cfg", big.c_str()));
  reboot();
  EXPECT_EQ(seq("cfg"), 2);  // not 1: the number of the unstored entry is never reused
}

TEST_F(CloudOplogTest, CorruptMetadataDisablesSyncForTheList) {
  seedSynced("cfg");
  SPIFFS.writeRaw(syncPath("cfg"), "{not json");
  reboot();
  EXPECT_FALSE(db.isListSyncEnabled("cfg"));
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  EXPECT_EQ(oplogLines("cfg").size(), 3u);  // nothing appended
}

TEST_F(CloudOplogTest, TornOplogLineIsIgnored) {
  seedSynced("cfg");
  SPIFFS.writeRaw(oplogPath("cfg"), SPIFFS.readRaw(oplogPath("cfg")) + "{\"seq\":9,\"op\":\"ad");
  reboot();
  EXPECT_EQ(seq("cfg"), 9);  // the counter never goes backwards
  EXPECT_EQ(pending("cfg"), 4);
}

TEST_F(CloudOplogTest, StaleRestoreFileIsRemovedOnLoad) {
  seedSynced("cfg");
  SPIFFS.writeRaw(restorePath("cfg"), "{\"name\":\"half");
  reboot();
  db.isListSyncEnabled("cfg");
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
}

// --------------------------------------------------------- rollover -----

TEST_F(CloudOplogTest, OplogIsCappedAndDropsOldestEntries) {
  config.maxOplogBytes = 160;
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  for (int i = 0; i < 10; i++) {
    ASSERT_TRUE(add("cfg", ("{\"v\":" + std::to_string(i) + "}").c_str()));
    EXPECT_LE(SPIFFS.readRaw(oplogPath("cfg")).size(), 160u);
  }
  auto lines = oplogLines("cfg");
  ASSERT_FALSE(lines.empty());
  EXPECT_LT(lines.size(), 10u);
  // newest entries are kept, oldest dropped, and every kept line is whole
  EXPECT_EQ(lines.back(), R"({"seq":10,"op":"add","doc":{"v":9}})");
  long expected = 10 - (long)lines.size() + 1;
  for (auto& line : lines) {
    EXPECT_EQ(DBOplog::parseSeq(String(line)), expected++);
    JsonDocument check;
    EXPECT_FALSE(deserializeJson(check, line)) << line;
  }
  EXPECT_EQ(seq("cfg"), 10);
  EXPECT_EQ(pending("cfg"), (int)lines.size());
}

TEST_F(CloudOplogTest, EntryLargerThanCapIsSkippedButNumbered) {
  config.maxOplogBytes = 100;
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"v":1})"));
  std::string big = "{\"blob\":\"" + std::string(120, 'x') + "\"}";
  ASSERT_TRUE(add("cfg", big.c_str()));
  ASSERT_TRUE(add("cfg", R"({"v":3})"));
  auto lines = oplogLines("cfg");
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(DBOplog::parseSeq(String(lines[0])), 1);
  EXPECT_EQ(DBOplog::parseSeq(String(lines[1])), 3);  // gap at 2 -> snapshot on next sync
  EXPECT_EQ(seq("cfg"), 3);
  EXPECT_EQ(db.getRecordCount("cfg"), 3);
}

TEST_F(CloudOplogTest, ParseSeq) {
  EXPECT_EQ(DBOplog::parseSeq("{\"seq\":42,\"op\":\"add\"}"), 42);
  EXPECT_EQ(DBOplog::parseSeq("{\"seq\":0}"), 0);
  EXPECT_EQ(DBOplog::parseSeq("{\"op\":\"add\",\"seq\":1}"), -1);
  EXPECT_EQ(DBOplog::parseSeq("{\"seq\":x}"), -1);
  EXPECT_EQ(DBOplog::parseSeq(""), -1);
  EXPECT_EQ(DBOplog::parseSeq("{\"seq\":-5}"), -1);
}

// -------------------------------------------------------- delete list ----

TEST_F(CloudOplogTest, DeleteListOfflineLeavesTombstone) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  EXPECT_TRUE(db.deleteList("cfg"));
  EXPECT_FALSE(SPIFFS.exists(listPath("cfg").c_str()));
  EXPECT_FALSE(SPIFFS.exists(oplogPath("cfg").c_str()));
  ASSERT_TRUE(SPIFFS.exists(syncPath("cfg").c_str()));
  EXPECT_FALSE(db.isListSyncEnabled("cfg"));
  JsonDocument i = info("cfg");
  EXPECT_EQ(i["inst"].as<std::string>(), "");
  EXPECT_EQ(i["history"].as<std::string>(), id);
  EXPECT_EQ(i["seq"].as<long>(), 4);
  EXPECT_EQ(publishedCount(), 0u);
  JsonDocument meta;
  ASSERT_FALSE(deserializeJson(meta, SPIFFS.readRaw(syncPath("cfg"))));
  EXPECT_EQ(meta["hist"].as<std::string>(), id);
}

TEST_F(CloudOplogTest, RecreatedListGetsNewInstanceAndKeepsHistory) {
  seedSynced("cfg");
  std::string old = inst("cfg");
  ASSERT_TRUE(db.deleteList("cfg"));
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(add("cfg", R"({"name":"new"})"));
  EXPECT_FALSE(SPIFFS.exists(oplogPath("cfg").c_str()));  // not synced until enabled again
  ASSERT_TRUE(db.enableListSync("cfg"));
  EXPECT_NE(inst("cfg"), old);
  EXPECT_EQ(info("cfg")["history"].as<std::string>(), old);
  EXPECT_FALSE(info("cfg")["lost"].as<bool>());
  EXPECT_EQ(seq("cfg"), 0);
  ASSERT_TRUE(add("cfg", R"({"name":"newer"})"));
  EXPECT_EQ(oplogLines("cfg").size(), 1u);
}

TEST_F(CloudOplogTest, HistoryAccumulatesEveryDeletedInstanceInOrderAndIsBounded) {
  std::vector<std::string> ids;
  ASSERT_TRUE(db.createList("cfg"));
  for (int i = 0; i < __QUARKDB_MAX_INSTANCE_HISTORY__ + 2; i++) {
    ASSERT_TRUE(db.enableListSync("cfg"));
    ids.push_back(inst("cfg"));
    ASSERT_TRUE(db.deleteList("cfg"));
    ASSERT_TRUE(db.createList("cfg"));
  }
  // the newest __QUARKDB_MAX_INSTANCE_HISTORY__ ids are kept, oldest first
  std::string expected;
  for (size_t i = ids.size() - __QUARKDB_MAX_INSTANCE_HISTORY__; i < ids.size(); i++) {
    if (!expected.empty()) expected += ",";
    expected += ids[i];
  }
  EXPECT_EQ(info("cfg")["history"].as<std::string>(), expected);
  reboot();
  EXPECT_EQ(info("cfg")["history"].as<std::string>(), expected);  // persisted
}

TEST_F(CloudOplogTest, ListCreatedWhileInstanceIsLiveIsMarkedLost) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  // the list file vanishes without deleteList() (corruption, manual removal)
  SPIFFS.remove(listPath("cfg").c_str());
  reboot();
  EXPECT_FALSE(info("cfg")["lost"].as<bool>());
  ASSERT_TRUE(db.createList("cfg"));
  EXPECT_TRUE(info("cfg")["lost"].as<bool>());
  EXPECT_EQ(inst("cfg"), id);          // same instance, the cloud copy is the complete one
  EXPECT_EQ(state("cfg"), "unknown");  // needs a query before anything is pushed
  reboot();
  EXPECT_TRUE(info("cfg")["lost"].as<bool>());  // persisted
  // a normal create of an unsynced list sets nothing
  ASSERT_TRUE(db.createList("plain"));
  JsonDocument none;
  EXPECT_FALSE(db.getSyncInfo("plain", &none));
}

TEST_F(CloudOplogTest, DeleteUnsyncedListLeavesNoTrace) {
  ASSERT_TRUE(db.createList("plain"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.deleteList("plain"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}

TEST_F(CloudOplogTest, DisableListSyncRemovesTombstone) {
  seedSynced("cfg");
  ASSERT_TRUE(db.deleteList("cfg"));
  EXPECT_TRUE(db.disableListSync("cfg"));
  EXPECT_TRUE(SPIFFS.paths().empty());
}
