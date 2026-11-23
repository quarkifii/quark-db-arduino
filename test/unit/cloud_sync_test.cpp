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

// Cloud backup protocol tests: the device side of the query/status
// handshake, delta propagation from the oplog, offline accumulation,
// snapshots in both directions, restore and deleted lists. The cloud is
// played by delivering messages through the mocked PubSubClient.
#include "cloud_test_helpers.h"

#include <filter/filter_processor.h>

#include <algorithm>
#include <string>
#include <vector>

// ------------------------------------------------------------ syncCloud ----

TEST_F(CloudSyncTest, SyncCloudSubscribesAndQueriesEachSyncedList) {
  seedSynced("cfg");
  ASSERT_TRUE(db.createList("plain"));
  ASSERT_TRUE(add("plain", R"({"x":1})"));
  EXPECT_EQ(connectAndSync(), 1);
  EXPECT_TRUE(mqtt.isSubscribed(kIn));
  ASSERT_EQ(publishedCount(), 1u);
  EXPECT_EQ(mqtt.published()[0].topic, kOut);
  JsonDocument q = message(0);
  EXPECT_EQ(q["type"].as<std::string>(), "query");
  EXPECT_EQ(q["list"].as<std::string>(), "cfg");
  EXPECT_EQ(q["inst"].as<std::string>(), inst("cfg"));
  EXPECT_EQ(q["seq"].as<long>(), 3);
  EXPECT_TRUE(q["dev"].isNull());
  EXPECT_EQ(state("cfg"), "queried");
}

TEST_F(CloudSyncTest, SyncCloudFailsWhenOfflineOrNotEnabled) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  EXPECT_EQ(db.syncCloud(), -1);  // enableSync() not called
  ASSERT_TRUE(enableSync());
  EXPECT_EQ(db.syncCloud(), -1);  // broker connection down
  EXPECT_EQ(publishedCount(), 0u);
  mqtt.connect("esp-1");
  mqtt.failSubscribe(true);
  EXPECT_EQ(db.syncCloud(), -1);
  mqtt.failSubscribe(false);
  EXPECT_EQ(db.syncCloud(), 1);
}

TEST_F(CloudSyncTest, SyncCloudWithNoSyncedListsDoesNothing) {
  ASSERT_TRUE(db.createList("plain"));
  ASSERT_TRUE(enableSync());
  EXPECT_EQ(connectAndSync(), 0);
  EXPECT_TRUE(mqtt.isSubscribed(kIn));
  EXPECT_EQ(publishedCount(), 0u);
}

TEST_F(CloudSyncTest, SyncCloudAfterRebootFindsListsOnFlash) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  reboot();
  EXPECT_EQ(connectAndSync(), 1);
  JsonDocument q = message(0);
  EXPECT_EQ(q["inst"].as<std::string>(), id);
  EXPECT_EQ(q["seq"].as<long>(), 3);
}

TEST_F(CloudSyncTest, ReconnectQueriesAgain) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  EXPECT_EQ(state("cfg"), "insync");
  mqtt.disconnect();
  clearPublished();
  EXPECT_EQ(connectAndSync(), 1);
  EXPECT_EQ(types(), (std::vector<std::string>{"query"}));
  EXPECT_EQ(state("cfg"), "queried");
}

// --------------------------------------------------------------- status ----

TEST_F(CloudSyncTest, StatusInSyncSendsNothing) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "insync");
  EXPECT_EQ(cloudSeq("cfg"), 3);
  EXPECT_EQ(pending("cfg"), 0);
}

TEST_F(CloudSyncTest, StatusBehindPushesMissingOpsInOrder) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 1)));
  ASSERT_EQ(types(), (std::vector<std::string>{"op", "op"}));
  JsonDocument op2 = message(0);
  EXPECT_EQ(op2["list"].as<std::string>(), "cfg");
  EXPECT_EQ(op2["inst"].as<std::string>(), inst("cfg"));
  EXPECT_EQ(op2["seq"].as<long>(), 2);
  EXPECT_EQ(op2["op"].as<std::string>(), "add");
  EXPECT_EQ(op2["doc"]["name"].as<std::string>(), "bob");
  JsonDocument op3 = message(1);
  EXPECT_EQ(op3["seq"].as<long>(), 3);
  EXPECT_EQ(op3["doc"]["name"].as<std::string>(), "carol");
  EXPECT_EQ(cloudSeq("cfg"), 3);
  EXPECT_EQ(pending("cfg"), 0);
  EXPECT_EQ(state("cfg"), "insync");
}

TEST_F(CloudSyncTest, OpMessagesCarryEveryOperationType) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"name":"alice","age":30})"));
  EXPECT_EQ(update("cfg", R"({"age":30})", R"({"name":"alice","age":31})"), 1);
  EXPECT_EQ(db.deleteRecords("cfg", R"({"age":31})"), 1);
  connectAndSync();
  clearPublished();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 0)));
  ASSERT_EQ(types(), (std::vector<std::string>{"op", "op", "op"}));
  EXPECT_EQ(payload(0), "{\"type\":\"op\",\"list\":\"cfg\",\"inst\":\"" + inst("cfg") + "\",\"mid\":" + std::to_string(mid(0)) +
                            ",\"seq\":1,\"op\":\"add\",\"doc\":{\"name\":\"alice\",\"age\":30}}");
  JsonDocument upd = message(1);
  EXPECT_EQ(upd["op"].as<std::string>(), "update");
  EXPECT_EQ(upd["sel"]["age"].as<int>(), 30);
  EXPECT_EQ(upd["doc"]["age"].as<int>(), 31);
  EXPECT_EQ(upd["n"].as<int>(), 1);
  JsonDocument del = message(2);
  EXPECT_EQ(del["op"].as<std::string>(), "delete");
  EXPECT_EQ(del["sel"]["age"].as<int>(), 31);
  EXPECT_EQ(del["n"].as<int>(), 1);
  EXPECT_TRUE(del["doc"].isNull());
}

TEST_F(CloudSyncTest, StatusWithoutInstanceSendsSnapshot) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  ASSERT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  JsonDocument snap = message(0);
  EXPECT_EQ(snap["list"].as<std::string>(), "cfg");
  EXPECT_EQ(snap["inst"].as<std::string>(), inst("cfg"));
  EXPECT_EQ(snap["seq"].as<long>(), 3);
  EXPECT_EQ(snap["i"].as<long>(), 0);
  EXPECT_TRUE(snap["last"].as<bool>());
  ASSERT_EQ(snap["docs"].size(), 3u);
  EXPECT_EQ(snap["docs"][0]["name"].as<std::string>(), "alice");
  EXPECT_EQ(snap["docs"][1]["name"].as<std::string>(), "bob");
  EXPECT_EQ(snap["docs"][2]["age"].as<int>(), 46);
  EXPECT_EQ(state("cfg"), "insync");
  EXPECT_EQ(cloudSeq("cfg"), 3);
  EXPECT_EQ(pending("cfg"), 0);
}

TEST_F(CloudSyncTest, StatusAheadOfDeviceSendsSnapshot) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 7)));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  EXPECT_EQ(message(0)["seq"].as<long>(), 3);
  EXPECT_EQ(cloudSeq("cfg"), 3);
}

TEST_F(CloudSyncTest, InstanceMismatchDefaultsToDeviceWins) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "some-other-instance", 50)));
  ASSERT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  EXPECT_EQ(message(0)["inst"].as<std::string>(), inst("cfg"));
  EXPECT_EQ(db.getRecordCount("cfg"), 3);  // local data untouched
  EXPECT_EQ(state("cfg"), "insync");
}

TEST_F(CloudSyncTest, InstanceMismatchWithRestoreOptionPullsCloudCopy) {
  config.restoreOnMismatch = true;
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "cloud-inst", 50)));
  ASSERT_EQ(types(), (std::vector<std::string>{"restore"}));
  JsonDocument req = message(0);
  EXPECT_EQ(req["list"].as<std::string>(), "cfg");
  EXPECT_EQ(req["inst"].as<std::string>(), inst("cfg"));
  EXPECT_EQ(state("cfg"), "restoring");
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "cloud-inst", 50, 0, true, R"([{"name":"zed","age":9}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"zed"}));
  EXPECT_EQ(inst("cfg"), "cloud-inst");
  EXPECT_EQ(seq("cfg"), 50);
  EXPECT_EQ(cloudSeq("cfg"), 50);
  EXPECT_EQ(state("cfg"), "insync");
  EXPECT_EQ(pending("cfg"), 0);
  EXPECT_FALSE(SPIFFS.exists(oplogPath("cfg").c_str()));
  // the adopted instance is persisted
  JsonDocument meta;
  ASSERT_FALSE(deserializeJson(meta, SPIFFS.readRaw(syncPath("cfg"))));
  EXPECT_EQ(meta["inst"].as<std::string>(), "cloud-inst");
  EXPECT_EQ(meta["seq"].as<long>(), 50);
  // and changes continue from the cloud sequence
  ASSERT_TRUE(add("cfg", R"({"name":"after"})"));
  EXPECT_EQ(message(1)["seq"].as<long>(), 51);
}

TEST_F(CloudSyncTest, PerListRestoreOptionOverridesGlobal) {
  config.restoreOnMismatch = true;
  ASSERT_TRUE(db.createList("never"));
  ASSERT_TRUE(db.createList("inherit"));
  ASSERT_TRUE(enableSync());
  QuarkListSyncOptions never;
  never.restore = QUARK_RESTORE_NEVER;
  ASSERT_TRUE(db.enableListSync("never", never));
  ASSERT_TRUE(db.enableListSync("inherit"));
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("never", "x", 1)));
  EXPECT_TRUE(cloudSends(statusJson("inherit", "x", 1)));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot", "restore"}));

  config.restoreOnMismatch = false;
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.createList("always"));
  QuarkListSyncOptions always;
  always.restore = QUARK_RESTORE_ON_MISMATCH;
  ASSERT_TRUE(db.enableListSync("always", always));
  db.syncCloud();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("always", "x", 1)));
  EXPECT_EQ(types(), (std::vector<std::string>{"restore"}));
}

TEST_F(CloudSyncTest, DropsFromRolloverSendSnapshotInsteadOfOps) {
  config.maxOplogBytes = 160;
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  for (int i = 0; i < 12; i++) ASSERT_TRUE(add("cfg", ("{\"v\":" + std::to_string(i) + "}").c_str()));
  ASSERT_LT(oplogLines("cfg").size(), 12u);
  connectAndSync();
  clearPublished();
  // cloud has seq 2, which rolled out of the oplog
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 2)));
  ASSERT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  JsonDocument snap = message(0);
  EXPECT_EQ(snap["seq"].as<long>(), 12);
  EXPECT_EQ(snap["docs"].size(), 12u);
  EXPECT_EQ(cloudSeq("cfg"), 12);
  // the entries still in the log are enough for a cloud that is only slightly behind
  ASSERT_TRUE(add("cfg", R"({"v":12})"));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot", "op"}));
  EXPECT_EQ(message(1)["seq"].as<long>(), 13);
}

TEST_F(CloudSyncTest, OversizedEntryGapSendsSnapshot) {
  config.maxOplogBytes = 100;
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  ASSERT_TRUE(add("cfg", R"({"v":1})"));
  std::string big = "{\"blob\":\"" + std::string(120, 'x') + "\"}";
  ASSERT_TRUE(add("cfg", big.c_str()));
  EXPECT_EQ(db.deleteRecords("cfg", R"({"blob":{"$eq":")" + std::string(120, 'x') + "\"}}"), 1);
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 0)));
  // seq 1 is sent, seq 2 is missing from the log, so a snapshot follows
  EXPECT_EQ(types(), (std::vector<std::string>{"op", "snapshot"}));
  EXPECT_EQ(message(1)["docs"].size(), 1u);
  EXPECT_EQ(message(1)["seq"].as<long>(), 3);
  EXPECT_EQ(cloudSeq("cfg"), 3);
}

TEST_F(CloudSyncTest, OpTooLargeForPayloadFallsBackToSnapshot) {
  config.maxPayloadBytes = 256;
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  std::string big = "{\"blob\":\"" + std::string(230, 'x') + "\"}";
  ASSERT_TRUE(add("cfg", big.c_str()));
  EXPECT_EQ(db.deleteRecords("cfg", "{}"), 1);
  ASSERT_TRUE(add("cfg", R"({"v":1})"));
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 0)));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  EXPECT_LE(payload(0).size(), 256u);
  EXPECT_EQ(message(0)["docs"].size(), 1u);
}

TEST_F(CloudSyncTest, TornOplogEntryIsNotReplayed) {
  seedSynced("cfg");
  SPIFFS.writeRaw(oplogPath("cfg"), SPIFFS.readRaw(oplogPath("cfg")) + "{\"seq\":4,\"op\":\"add\",\"doc\":{\"na");
  reboot();
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 2)));
  EXPECT_EQ(types(), (std::vector<std::string>{"op", "snapshot"}));
  for (size_t i = 0; i < publishedCount(); i++) {
    JsonDocument check;
    EXPECT_FALSE(deserializeJson(check, payload(i))) << payload(i);
  }
}

// ----------------------------------------------------------- live push ----

TEST_F(CloudSyncTest, ChangesArePublishedImmediatelyWhileInSync) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"dave","age":24})"));
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["seq"].as<long>(), 4);
  EXPECT_EQ(message(0)["op"].as<std::string>(), "add");
  EXPECT_EQ(message(0)["doc"]["name"].as<std::string>(), "dave");
  EXPECT_EQ(update("cfg", R"({"name":"dave"})", R"({"name":"dave","age":25})"), 1);
  EXPECT_EQ(db.deleteRecords("cfg", R"({"name":"alice"})"), 1);
  ASSERT_EQ(types(), (std::vector<std::string>{"op", "op", "op"}));
  EXPECT_EQ(message(1)["op"].as<std::string>(), "update");
  EXPECT_EQ(message(1)["seq"].as<long>(), 5);
  EXPECT_EQ(message(2)["op"].as<std::string>(), "delete");
  EXPECT_EQ(message(2)["seq"].as<long>(), 6);
  EXPECT_EQ(cloudSeq("cfg"), 6);
  EXPECT_EQ(pending("cfg"), 0);
}

TEST_F(CloudSyncTest, CliChangesArePublishedToo) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  clearPublished();
  Serial.reset();
  Serial.feed("QUARKDB>db.cfg.add({\"name\":\"cli\"})\n");
  db.processSerialCommand();
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["doc"]["name"].as<std::string>(), "cli");
}

TEST_F(CloudSyncTest, ChangesWhileOfflineAccumulateAndFlushAfterReconnect) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 0)));
  EXPECT_EQ(types(), (std::vector<std::string>{"query", "op", "op", "op"}));
  mqtt.disconnect();
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  ASSERT_TRUE(add("cfg", R"({"name":"erin"})"));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(pending("cfg"), 2);
  EXPECT_EQ(oplogLines("cfg").size(), 5u);
  EXPECT_EQ(connectAndSync(), 1);
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  ASSERT_EQ(types(), (std::vector<std::string>{"query", "op", "op"}));
  EXPECT_EQ(message(1)["seq"].as<long>(), 4);
  EXPECT_EQ(message(2)["seq"].as<long>(), 5);
  EXPECT_EQ(message(2)["doc"]["name"].as<std::string>(), "erin");
  EXPECT_EQ(pending("cfg"), 0);
}

TEST_F(CloudSyncTest, FailedPublishStaysPendingAndIsRetriedOnNextChange) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  clearPublished();
  mqtt.failNextPublishes(1);
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(cloudSeq("cfg"), 3);
  EXPECT_EQ(pending("cfg"), 1);
  ASSERT_TRUE(add("cfg", R"({"name":"erin"})"));
  ASSERT_EQ(types(), (std::vector<std::string>{"op", "op"}));
  EXPECT_EQ(message(0)["seq"].as<long>(), 4);
  EXPECT_EQ(message(1)["seq"].as<long>(), 5);
  EXPECT_EQ(cloudSeq("cfg"), 5);
}

TEST_F(CloudSyncTest, ChangeWithUnknownCloudStateQueriesFirst) {
  seedSynced("cfg");
  mqtt.connect("esp-1");  // connected, but syncCloud() not called yet
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  ASSERT_EQ(types(), (std::vector<std::string>{"query"}));
  EXPECT_EQ(message(0)["seq"].as<long>(), 4);
  EXPECT_EQ(state("cfg"), "queried");
  // while waiting for the answer nothing else is sent
  ASSERT_TRUE(add("cfg", R"({"name":"erin"})"));
  EXPECT_EQ(publishedCount(), 1u);
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  EXPECT_EQ(types(), (std::vector<std::string>{"query", "op", "op"}));
}

TEST_F(CloudSyncTest, DisableSyncStopsPublishingButKeepsLogging) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  db.disableSync();
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(oplogLines("cfg").size(), 4u);
  EXPECT_FALSE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  EXPECT_EQ(db.syncCloud(), -1);
  EXPECT_EQ(db.restoreFromCloud("cfg"), -1);
}

// ------------------------------------------------------------------ ack ----

TEST_F(CloudSyncTest, EveryPublishedMessageCarriesAUniqueMessageId) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", "", 0)));   // snapshot
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));          // op
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);                // restore
  ASSERT_GE(publishedCount(), 4u);
  std::vector<unsigned long> mids;
  for (size_t i = 0; i < publishedCount(); i++) {
    EXPECT_TRUE(message(i)["mid"].is<unsigned long>()) << payload(i);
    unsigned long m = mid(i);
    EXPECT_NE(m, 0u);
    for (unsigned long seen : mids) EXPECT_NE(seen, m);
    mids.push_back(m);
  }
  JsonDocument i = info("cfg");
  EXPECT_EQ(i["restoreMid"].as<unsigned long>(), mids.back());
  EXPECT_EQ(i["queryMid"].as<unsigned long>(), 0u);  // the query was answered
}

TEST_F(CloudSyncTest, AcksCorrelateByMessageIdInAnyOrder) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 0)));
  clearPublished();
  // three ops published in sync: seq 4, 5, 6 with their own mids
  ASSERT_TRUE(add("cfg", R"({"name":"d"})"));
  ASSERT_TRUE(add("cfg", R"({"name":"e"})"));
  ASSERT_TRUE(add("cfg", R"({"name":"f"})"));
  ASSERT_EQ(types(), (std::vector<std::string>{"op", "op", "op"}));
  unsigned long m4 = mid(0), m5 = mid(1), m6 = mid(2);
  // the device assumed delivery optimistically; acks only confirm
  EXPECT_EQ(cloudSeq("cfg"), 6);
  // simulate a cloud that was in fact behind, then acks out of order
  db.syncCloud();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));  // re-sends 4,5,6 with new mids
  clearPublished();
  EXPECT_EQ(cloudSeq("cfg"), 6);
  // an ack for a mid this instance never published changes nothing
  EXPECT_FALSE(cloudSends(ackJson("cfg", 123456789UL)));
  EXPECT_FALSE(cloudSends(ackJson("cfg", 0)));
  EXPECT_FALSE(cloudSends("{\"type\":\"ack\",\"list\":\"cfg\"}"));
  EXPECT_FALSE(cloudSends("{\"type\":\"ack\",\"list\":\"cfg\",\"mid\":\"" + std::to_string(m5) + "\"}"));  // wrong type
  // older mids are still known (window of recent messages) and map to their seq
  EXPECT_TRUE(cloudSends(ackJson("cfg", m5)));
  EXPECT_TRUE(cloudSends(ackJson("cfg", m4)));
  EXPECT_TRUE(cloudSends(ackJson("cfg", m6)));
  EXPECT_EQ(cloudSeq("cfg"), 6);
  EXPECT_EQ(publishedCount(), 0u);  // acks never cause traffic
  // an ack naming another instance is refused
  EXPECT_FALSE(cloudSends(ackJson("cfg", m6, ",\"inst\":\"other\"")));
  EXPECT_TRUE(cloudSends(ackJson("cfg", m6, ",\"inst\":\"" + inst("cfg") + "\"")));
}

TEST_F(CloudSyncTest, AckMovesPositionForwardOnlyAfterAFailedPublish) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"d"})"));   // seq 4 published, mid m4
  unsigned long m4 = mid(0);
  mqtt.failNextPublishes(1);
  ASSERT_TRUE(add("cfg", R"({"name":"e"})"));   // seq 5 not published
  EXPECT_EQ(cloudSeq("cfg"), 4);
  EXPECT_EQ(pending("cfg"), 1);
  EXPECT_TRUE(cloudSends(ackJson("cfg", m4)));  // confirms 4, does not pretend 5 arrived
  EXPECT_EQ(cloudSeq("cfg"), 4);
  EXPECT_EQ(pending("cfg"), 1);
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"f"})"));   // catches up: 5 then 6
  ASSERT_EQ(types(), (std::vector<std::string>{"op", "op"}));
  EXPECT_EQ(message(0)["seq"].as<long>(), 5);
  unsigned long m6 = mid(1);
  EXPECT_TRUE(cloudSends(ackJson("cfg", m6)));
  EXPECT_EQ(cloudSeq("cfg"), 6);
}

TEST_F(CloudSyncTest, AckOfTheLastSnapshotChunkConfirmsTheWholeSnapshot) {
  config.maxPayloadBytes = 220;
  seedSynced("cfg");
  for (int i = 0; i < 6; i++) ASSERT_TRUE(add("cfg", ("{\"name\":\"padding-padding-" + std::to_string(i) + "\"}").c_str()));
  connectAndSync();
  clearPublished();
  ASSERT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  ASSERT_GT(publishedCount(), 1u);
  for (size_t i = 0; i + 1 < publishedCount(); i++) {
    EXPECT_FALSE(cloudSends(ackJson("cfg", mid(i))));  // intermediate chunks commit nothing
  }
  EXPECT_TRUE(cloudSends(ackJson("cfg", mid(publishedCount() - 1))));
  EXPECT_EQ(cloudSeq("cfg"), 9);
}

TEST_F(CloudSyncTest, AcksOfAPreviousInstanceAreForgottenAfterRestore) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 0)));
  unsigned long oldOpMid = mid(1);
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  ASSERT_TRUE(cloudSends(snapshotJson("cfg", "x", 40, 0, true, R"([{"name":"c1"}])")));
  EXPECT_EQ(cloudSeq("cfg"), 40);
  EXPECT_FALSE(cloudSends(ackJson("cfg", oldOpMid)));
  EXPECT_EQ(cloudSeq("cfg"), 40);
}

TEST_F(CloudSyncTest, StatusMustCarryTheMidOfTheOpenQuery) {
  config.queryTimeoutMs = 60000;
  seedSynced("cfg");
  connectAndSync();
  unsigned long firstQuery = mid(0);
  // no mid, wrong mid, mid of the wrong type: all ignored, the device keeps waiting
  EXPECT_FALSE(cloudSends("{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"\",\"seq\":0}"));
  EXPECT_FALSE(cloudSends("{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"\",\"seq\":0,\"mid\":" + std::to_string(firstQuery + 1) + "}"));
  EXPECT_FALSE(cloudSends("{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"\",\"seq\":0,\"mid\":\"" + std::to_string(firstQuery) + "\"}"));
  EXPECT_EQ(publishedCount(), 1u);
  EXPECT_EQ(state("cfg"), "queried");
  // the answer never came; a change after the timeout asks again with a new mid
  advanceMillis(60000);
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  ASSERT_EQ(types(), (std::vector<std::string>{"query", "query"}));
  unsigned long secondQuery = mid(1);
  EXPECT_NE(secondQuery, firstQuery);
  clearPublished();
  // the late answer to the first query must not be mistaken for the second
  EXPECT_FALSE(cloudSends("{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"\",\"seq\":0,\"mid\":" + std::to_string(firstQuery) + "}"));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "queried");
  EXPECT_TRUE(cloudSends("{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"" + inst("cfg") + "\",\"seq\":3,\"mid\":" + std::to_string(secondQuery) + "}"));
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["seq"].as<long>(), 4);
  // and the same answer delivered twice is acted on once
  clearPublished();
  EXPECT_FALSE(cloudSends("{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"" + inst("cfg") + "\",\"seq\":3,\"mid\":" + std::to_string(secondQuery) + "}"));
  EXPECT_EQ(publishedCount(), 0u);
}

TEST_F(CloudSyncTest, SnapshotChunksMustCarryTheMidOfTheRestoreRequest) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  unsigned long first = mid(0);
  // chunks of an earlier (superseded) restore are refused once a new one was requested
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  unsigned long second = mid(1);
  EXPECT_NE(first, second);
  std::string chunkFor = "{\"type\":\"snapshot\",\"list\":\"cfg\",\"inst\":\"x\",\"seq\":9,\"i\":0,\"last\":true,\"docs\":[{\"name\":\"c1\"}],\"count\":1,\"crc\":" +
                         std::to_string(crcOfDocs({"{\"name\":\"c1\"}"})) + ",\"mid\":";
  EXPECT_FALSE(cloudSends(chunkFor + std::to_string(first) + "}"));
  EXPECT_FALSE(cloudSends("{\"type\":\"snapshot\",\"list\":\"cfg\",\"inst\":\"x\",\"seq\":9,\"i\":0,\"last\":true,\"docs\":[{\"name\":\"c1\"}]}"));
  EXPECT_EQ(state("cfg"), "restoring");
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));
  EXPECT_TRUE(cloudSends(chunkFor + std::to_string(second) + "}"));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"c1"}));
  EXPECT_EQ(info("cfg")["restoreMid"].as<unsigned long>(), 0u);
}

// -------------------------------------------------------- handleMessage ----

TEST_F(CloudSyncTest, HandleMessageRejectsWhatIsNotForIt) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_FALSE(db.handleMessage("devices/esp-2/quarkdb/in", String(statusJson("cfg", inst("cfg"), 0).c_str())));
  EXPECT_FALSE(db.handleMessage(kOut, String(statusJson("cfg", inst("cfg"), 0).c_str())));
  EXPECT_FALSE(cloudSends("{not json"));
  EXPECT_FALSE(cloudSends("[1,2]"));
  EXPECT_FALSE(cloudSends(""));
  EXPECT_FALSE(cloudSends(R"({"type":"status","inst":"x","seq":1})"));          // no list
  EXPECT_FALSE(cloudSends(statusJson("ghost", "x", 1)));                         // unknown list
  EXPECT_FALSE(cloudSends(statusJson("bad name", "x", 1)));
  EXPECT_FALSE(cloudSends(R"({"type":"hello","list":"cfg"})"));                  // unknown type
  EXPECT_FALSE(cloudSends(R"({"list":"cfg","inst":"x"})"));                      // no type
  EXPECT_FALSE(db.handleMessage(nullptr, String("{}")));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "queried");
  // a status for an unsynced list is ignored too
  ASSERT_TRUE(db.createList("plain"));
  EXPECT_FALSE(cloudSends(statusJson("plain", "", 0)));
}

TEST_F(CloudSyncTest, HandleMessageStringOverload) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(db.handleMessage(kIn, String(statusJson("cfg", inst("cfg"), 2).c_str())));
  EXPECT_EQ(types(), (std::vector<std::string>{"op"}));
}

TEST_F(CloudSyncTest, HandleMessageRawPayloadUsesOnlyLengthBytes) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  std::string json = statusJson("cfg", inst("cfg"), 2);
  std::string buffer = json + "garbage that is outside the length";
  EXPECT_TRUE(db.handleMessage(kIn, reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<unsigned int>(json.size())));
  EXPECT_EQ(types(), (std::vector<std::string>{"op"}));
}

TEST_F(CloudSyncTest, HandleMessageRawNullPayloadIsRejected) {
  seedSynced("cfg");
  connectAndSync();
  EXPECT_FALSE(db.handleMessage(kIn, nullptr, 10));
  EXPECT_FALSE(db.handleMessage(kIn, reinterpret_cast<const uint8_t*>("{}"), 0));
}

TEST_F(CloudSyncTest, WildcardSubscribeTopic) {
  config.subscribeTopic = "devices/+/quarkdb/in";
  seedSynced("cfg");
  connectAndSync();
  EXPECT_TRUE(mqtt.isSubscribed("devices/+/quarkdb/in"));
  clearPublished();
  EXPECT_TRUE(db.handleMessage("devices/esp-1/quarkdb/in", String(statusJson("cfg", inst("cfg"), 2).c_str())));
  EXPECT_FALSE(db.handleMessage("devices/esp-1/other/in", String(statusJson("cfg", inst("cfg"), 2).c_str())));
  EXPECT_EQ(types(), (std::vector<std::string>{"op"}));
}

TEST_F(CloudSyncTest, DeviceIdIsSentAndFiltered) {
  config.deviceId = "esp\"1";
  seedSynced("cfg");
  connectAndSync();
  EXPECT_EQ(message(0)["dev"].as<std::string>(), "esp\"1");
  clearPublished();
  EXPECT_FALSE(cloudSends(statusJson("cfg", "", 0, ",\"dev\":\"esp-2\"")));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0, ",\"dev\":\"esp\\\"1\"")));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  db.syncCloud();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));  // no dev field: accepted
}

// ------------------------------------------------------- snapshot out ----

TEST_F(CloudSyncTest, SnapshotIsChunkedToPayloadLimit) {
  config.maxPayloadBytes = 220;
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  for (int i = 0; i < 10; i++) {
    ASSERT_TRUE(add("cfg", ("{\"name\":\"record-" + std::to_string(i) + "\",\"value\":" + std::to_string(i * 11) + "}").c_str()));
  }
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  ASSERT_GT(publishedCount(), 1u);
  int seen = 0;
  for (size_t i = 0; i < publishedCount(); i++) {
    EXPECT_LE(payload(i).size(), 220u) << payload(i);
    JsonDocument chunk = message(i);
    EXPECT_EQ(chunk["type"].as<std::string>(), "snapshot");
    EXPECT_EQ(chunk["inst"].as<std::string>(), inst("cfg"));
    EXPECT_EQ(chunk["seq"].as<long>(), 10);
    EXPECT_EQ(chunk["i"].as<long>(), (long)i);
    EXPECT_EQ(chunk["last"].as<bool>(), i + 1 == publishedCount());
    JsonArray docs = chunk["docs"].as<JsonArray>();
    ASSERT_FALSE(docs.isNull());
    EXPECT_GT(docs.size(), 0u);
    for (JsonVariant doc : docs) {
      EXPECT_EQ(doc["name"].as<std::string>(), "record-" + std::to_string(seen));
      EXPECT_EQ(doc["value"].as<int>(), seen * 11);
      seen++;
    }
  }
  EXPECT_EQ(seen, 10);
  EXPECT_EQ(state("cfg"), "insync");
}

TEST_F(CloudSyncTest, SnapshotChunksFitTheClientBuffer) {
  // maxPayloadBytes (1024) is larger than what the PubSubClient buffer allows
  mqtt.setBufferSize(256);
  seedSynced("cfg");
  for (int i = 0; i < 10; i++) ASSERT_TRUE(add("cfg", ("{\"name\":\"padding-padding-" + std::to_string(i) + "\"}").c_str()));
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  EXPECT_EQ(mqtt.failedPublishes(), 0);
  ASSERT_GT(publishedCount(), 1u);
  size_t limit = transport.maxPayloadSize(kOut);
  size_t docs = 0;
  for (size_t i = 0; i < publishedCount(); i++) {
    EXPECT_LE(payload(i).size(), limit);
    docs += message(i)["docs"].size();
  }
  EXPECT_EQ(docs, 13u);
  EXPECT_TRUE(message(publishedCount() - 1)["last"].as<bool>());
}

TEST_F(CloudSyncTest, EmptyListSnapshot) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  ASSERT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  JsonDocument snap = message(0);
  EXPECT_EQ(snap["i"].as<long>(), 0);
  EXPECT_TRUE(snap["last"].as<bool>());
  EXPECT_EQ(snap["seq"].as<long>(), 0);
  EXPECT_TRUE(snap["docs"].is<JsonArray>());
  EXPECT_EQ(snap["docs"].size(), 0u);
}

TEST_F(CloudSyncTest, SnapshotSkipsCorruptAndBlankLines) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  SPIFFS.writeRaw(listPath("cfg"), "{\"name\":\"ok1\"}\r\n\r\n{broken\r\n{\"name\":\"ok2\"}\r\n{\"name\":\"to");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  JsonDocument snap = message(0);
  ASSERT_EQ(snap["docs"].size(), 2u);
  EXPECT_EQ(snap["docs"][0]["name"].as<std::string>(), "ok1");
  EXPECT_EQ(snap["docs"][1]["name"].as<std::string>(), "ok2");
}

TEST_F(CloudSyncTest, SnapshotPublishFailureLeavesStateUnknown) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  mqtt.failNextPublishes(1);
  EXPECT_FALSE(cloudSends(statusJson("cfg", "", 0)));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_EQ(cloudSeq("cfg"), -1);
  // a later change asks again instead of pushing blindly
  ASSERT_TRUE(add("cfg", R"({"name":"dave"})"));
  EXPECT_EQ(types(), (std::vector<std::string>{"query"}));
}

TEST_F(CloudSyncTest, SnapshotOfMissingListFails) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  SPIFFS.remove(listPath("cfg").c_str());
  EXPECT_FALSE(cloudSends(statusJson("cfg", "", 0)));
  EXPECT_EQ(publishedCount(), 0u);
}

// --------------------------------------------------------------- restore ----

TEST_F(CloudSyncTest, RestoreFromCloudOnDemand) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  EXPECT_EQ(db.restoreFromCloud("cfg"), 1);
  ASSERT_EQ(types(), (std::vector<std::string>{"restore"}));
  JsonDocument req = message(0);
  EXPECT_EQ(req["list"].as<std::string>(), "cfg");
  EXPECT_EQ(req["inst"].as<std::string>(), inst("cfg"));
  EXPECT_EQ(req["seq"].as<long>(), 3);
  EXPECT_EQ(state("cfg"), "restoring");
  std::string cloudInst = inst("cfg");  // same instance: the cloud echoes what it stores
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", cloudInst, 3, 0, false, R"([{"name":"c1"},{"name":"c2"}])")));
  EXPECT_TRUE(SPIFFS.exists(restorePath("cfg").c_str()));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));  // untouched until the last chunk
  EXPECT_EQ(state("cfg"), "restoring");
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", cloudInst, 3, 1, true, R"([{"name":"c3"}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"c1", "c2", "c3"}));
  EXPECT_EQ(SPIFFS.readRaw(listPath("cfg")), "{\"name\":\"c1\"}\r\n{\"name\":\"c2\"}\r\n{\"name\":\"c3\"}\r\n");
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
  EXPECT_FALSE(SPIFFS.exists(newPath("cfg").c_str()));
  EXPECT_FALSE(SPIFFS.exists(oplogPath("cfg").c_str()));
  EXPECT_EQ(state("cfg"), "insync");
  EXPECT_EQ(seq("cfg"), 3);
  EXPECT_EQ(cloudSeq("cfg"), 3);
  EXPECT_EQ(pending("cfg"), 0);
  // the restored list keeps working as a synced list
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"c4"})"));
  EXPECT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["seq"].as<long>(), 4);
  EXPECT_EQ(db.getRecordCount("cfg"), 4);
}

TEST_F(CloudSyncTest, RestoreFromCloudErrors) {
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.createList("plain"));
  EXPECT_EQ(db.restoreFromCloud("cfg"), -1);  // sync not enabled
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  EXPECT_EQ(db.restoreFromCloud("plain"), -1);  // list not synced
  EXPECT_EQ(db.restoreFromCloud("ghost"), -1);
  EXPECT_EQ(db.restoreFromCloud("bad name"), -1);
  EXPECT_EQ(db.restoreFromCloud("cfg"), 0);    // offline
  EXPECT_EQ(state("cfg"), "unknown");
  mqtt.connect("esp-1");
  mqtt.failNextPublishes(1);
  EXPECT_EQ(db.restoreFromCloud("cfg"), 0);
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_EQ(db.restoreFromCloud("cfg"), 1);
}

TEST_F(CloudSyncTest, RestoreIntoEmptyListAfterWipe) {
  // Flash was wiped: the list is recreated empty and pulled from the cloud
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("cfg"));
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "cloud-inst", 17, 0, true, R"([{"k":"v"},{"k":"w"}])")));
  EXPECT_EQ(db.getRecordCount("cfg"), 2);
  EXPECT_EQ(inst("cfg"), "cloud-inst");
  EXPECT_EQ(seq("cfg"), 17);
}

TEST_F(CloudSyncTest, UnsolicitedSnapshotIsIgnored) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", inst("cfg"), 3, 0, true, R"([{"name":"evil"}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
}

TEST_F(CloudSyncTest, OutOfOrderChunkAbortsRestore) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 3, 0, false, R"([{"name":"c1"}])")));
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 3, 2, true, R"([{"name":"c3"}])")));  // chunk 1 lost
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));
  // nothing is accepted until a new restore is requested
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 3, 0, true, R"([{"name":"c1"}])")));
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 3, 0, true, R"([{"name":"c1"}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"c1"}));
}

TEST_F(CloudSyncTest, SnapshotChunkMustCarryInstanceAndObjects) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "", 3, 0, true, R"([{"name":"c1"}])")));
  EXPECT_EQ(state("cfg"), "unknown");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  // non-object entries are skipped, the rest is installed
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 3, 0, true, R"([{"name":"c1"},5,"str",null,{"name":"c2"}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"c1", "c2"}));
}

TEST_F(CloudSyncTest, RestoreReplacesChangesMadeWhileWaiting) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"meanwhile"})"));
  EXPECT_EQ(publishedCount(), 0u);  // not pushed while a restore is pending
  EXPECT_EQ(seq("cfg"), 4);
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 9, 0, true, R"([{"name":"c1"}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"c1"}));
  EXPECT_EQ(seq("cfg"), 9);
  EXPECT_EQ(pending("cfg"), 0);
}

TEST_F(CloudSyncTest, RestoreSurvivesPowerLossDuringInstall) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  SPIFFS.powerLossOnRemove(/*partial=*/true);  // cut while the old list file is being removed
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 0, true, R"([{"name":"c1"},{"name":"c2"}])")));
  ASSERT_TRUE(SPIFFS.exists(newPath("cfg").c_str()));
  reboot();
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"c1", "c2"}));
  EXPECT_FALSE(SPIFFS.exists(newPath("cfg").c_str()));
  // the metadata was not updated yet, so the next sync starts with a query
  EXPECT_EQ(inst("cfg"), id);
  clearPublished();
  EXPECT_EQ(connectAndSync(), 1);
  EXPECT_EQ(types(), (std::vector<std::string>{"query"}));
}

TEST_F(CloudSyncTest, RestoreStagingFailureAborts) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  SPIFFS.failOpen("w");
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 0, true, R"([{"name":"c1"}])")));
  SPIFFS.failOpen("w", false);
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));
}

// ------------------------------------------------------------ drop list ----

TEST_F(CloudSyncTest, DeleteListOnlinePublishesDropAndForgetsSyncState) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", id, 3)));
  clearPublished();
  EXPECT_TRUE(db.deleteList("cfg"));
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  JsonDocument drop = message(0);
  EXPECT_EQ(drop["op"].as<std::string>(), "drop");
  EXPECT_EQ(drop["list"].as<std::string>(), "cfg");
  EXPECT_EQ(drop["inst"].as<std::string>(), id);
  EXPECT_EQ(drop["seq"].as<long>(), 4);
  EXPECT_TRUE(SPIFFS.paths().empty());
  EXPECT_FALSE(db.isListSyncEnabled("cfg"));
}

TEST_F(CloudSyncTest, TombstoneIsResolvedByNextSyncCloud) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  EXPECT_TRUE(db.deleteList("cfg"));
  ASSERT_TRUE(SPIFFS.exists(syncPath("cfg").c_str()));
  reboot();
  // the device does not guess what the cloud holds: it asks
  EXPECT_EQ(connectAndSync(), 1);
  ASSERT_EQ(types(), (std::vector<std::string>{"query"}));
  EXPECT_EQ(message(0)["inst"].as<std::string>(), "");
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", id, 3)));
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["op"].as<std::string>(), "drop");
  EXPECT_EQ(message(0)["inst"].as<std::string>(), id);
  EXPECT_EQ(message(0)["seq"].as<long>(), 4);
  EXPECT_TRUE(SPIFFS.paths().empty());
  clearPublished();
  EXPECT_EQ(connectAndSync(), 0);
  EXPECT_EQ(publishedCount(), 0u);
}

TEST_F(CloudSyncTest, TombstoneDropsOnlyAnInstanceItDeleted) {
  seedSynced("cfg");
  std::string first = inst("cfg");
  ASSERT_TRUE(db.deleteList("cfg"));
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  std::string second = inst("cfg");
  ASSERT_TRUE(db.deleteList("cfg"));
  EXPECT_EQ(info("cfg")["history"].as<std::string>(), first + "," + second);
  // cloud never heard about the second incarnation and still holds the first
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", first, 3)));
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["op"].as<std::string>(), "drop");
  EXPECT_EQ(message(0)["inst"].as<std::string>(), first);
  EXPECT_TRUE(SPIFFS.paths().empty());

  // cloud holds something this device never created: left alone, tombstone cleared
  mqtt.disconnect();
  seedSynced("two");
  ASSERT_TRUE(db.deleteList("two"));
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("two", "foreign", 9)));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_TRUE(SPIFFS.paths().empty());

  // cloud has nothing: tombstone just goes away
  mqtt.disconnect();
  seedSynced("three");
  ASSERT_TRUE(db.deleteList("three"));
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("three", "", 0)));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_TRUE(SPIFFS.paths().empty());
}

TEST_F(CloudSyncTest, DeletedInstancesAreNeverRestoredEvenWhenAllowed) {
  config.restoreOnMismatch = true;
  // three incarnations while offline, the cloud only ever saw the first
  seedSynced("cfg");
  std::string first = inst("cfg");
  ASSERT_TRUE(db.deleteList("cfg"));
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  std::string second = inst("cfg");
  ASSERT_TRUE(db.deleteList("cfg"));
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  std::string third = inst("cfg");
  ASSERT_TRUE(add("cfg", R"({"name":"fresh"})"));
  EXPECT_EQ(info("cfg")["history"].as<std::string>(), first + "," + second);
  EXPECT_EQ(connectAndSync(), 1);
  EXPECT_EQ(message(0)["inst"].as<std::string>(), third);
  clearPublished();
  // the cloud offers the first incarnation: deliberately deleted, so the
  // current list replaces it instead of being overwritten by it
  EXPECT_TRUE(cloudSends(statusJson("cfg", first, 3)));
  ASSERT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  EXPECT_EQ(message(0)["inst"].as<std::string>(), third);
  EXPECT_EQ(message(0)["docs"][0]["name"].as<std::string>(), "fresh");
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"fresh"}));
  // the cloud now holds the current instance: the history is moot
  EXPECT_EQ(info("cfg")["history"].as<std::string>(), "");
  // the same after a reboot (history persisted), with the cloud holding the
  // third incarnation that was deleted while offline
  mqtt.disconnect();
  reboot();
  ASSERT_TRUE(db.deleteList("cfg"));
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(db.enableListSync("cfg"));
  EXPECT_EQ(info("cfg")["history"].as<std::string>(), third);
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", third, 4)));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  // an instance the device never knew follows the restore option as usual
  db.syncCloud();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "never-seen", 1)));
  EXPECT_EQ(types(), (std::vector<std::string>{"restore"}));
}

TEST_F(CloudSyncTest, LostListIsRestoredFromTheCloudWhenAllowed) {
  config.restoreOnMismatch = true;
  seedSynced("cfg");
  std::string id = inst("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", "", 0)));  // cloud now holds the 3 records at seq 3
  // the list file is lost (not deleted through the API) and created again
  SPIFFS.remove(listPath("cfg").c_str());
  reboot();
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(add("cfg", R"({"name":"after-loss"})"));  // seq 4, logged but not pushed
  EXPECT_EQ(connectAndSync(), 1);
  EXPECT_EQ(message(0)["inst"].as<std::string>(), id);  // same instance as the cloud
  clearPublished();
  // same instance, but the device knows its copy is not the original: pull
  EXPECT_TRUE(cloudSends(statusJson("cfg", id, 3)));
  ASSERT_EQ(types(), (std::vector<std::string>{"restore"}));
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", id, 3, 0, true, R"([{"name":"alice","age":30},{"name":"bob","age":43},{"name":"carol","age":46}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));
  EXPECT_FALSE(info("cfg")["lost"].as<bool>());
  EXPECT_EQ(seq("cfg"), 3);
  EXPECT_EQ(state("cfg"), "insync");
}

TEST_F(CloudSyncTest, LostListWithoutRestoreOptionIsPushedAsSnapshot) {
  seedSynced("cfg");
  std::string id = inst("cfg");
  SPIFFS.remove(listPath("cfg").c_str());
  reboot();
  ASSERT_TRUE(db.createList("cfg"));
  ASSERT_TRUE(add("cfg", R"({"name":"only"})"));
  connectAndSync();
  // acks are meaningless while the copy is known to be incomplete
  EXPECT_FALSE(cloudSends(ackJson("cfg", mid(0))));
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", id, 3)));
  ASSERT_EQ(types(), (std::vector<std::string>{"snapshot"}));  // never deltas on top of a lost list
  EXPECT_EQ(message(0)["docs"].size(), 1u);
  EXPECT_EQ(message(0)["seq"].as<long>(), 4);
  EXPECT_FALSE(info("cfg")["lost"].as<bool>());
  EXPECT_EQ(state("cfg"), "insync");
}

// ------------------------------------------------------ multiple lists ----

TEST_F(CloudSyncTest, ListsSyncIndependently) {
  ASSERT_TRUE(db.createList("a"));
  ASSERT_TRUE(db.createList("b"));
  ASSERT_TRUE(db.createList("plain"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("a"));
  ASSERT_TRUE(db.enableListSync("b"));
  ASSERT_TRUE(add("a", R"({"x":1})"));
  ASSERT_TRUE(add("b", R"({"y":1})"));
  ASSERT_TRUE(add("plain", R"({"z":1})"));
  EXPECT_EQ(connectAndSync(), 2);
  std::vector<std::string> queried;
  for (size_t i = 0; i < publishedCount(); i++) {
    EXPECT_EQ(type(i), "query");
    queried.push_back(message(i)["list"].as<std::string>());
  }
  std::sort(queried.begin(), queried.end());
  EXPECT_EQ(queried, (std::vector<std::string>{"a", "b"}));
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("a", inst("a"), 0)));
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["list"].as<std::string>(), "a");
  EXPECT_EQ(message(0)["doc"]["x"].as<int>(), 1);
  EXPECT_EQ(state("a"), "insync");
  EXPECT_EQ(state("b"), "queried");
  clearPublished();
  ASSERT_TRUE(add("b", R"({"y":2})"));
  EXPECT_EQ(publishedCount(), 0u);
  ASSERT_TRUE(add("a", R"({"x":2})"));
  ASSERT_EQ(types(), (std::vector<std::string>{"op"}));
  EXPECT_EQ(message(0)["list"].as<std::string>(), "a");
}

// ------------------------------------------------------------ sync info ----

TEST_F(CloudSyncTest, SyncInfoReportsState) {
  JsonDocument none;
  EXPECT_FALSE(db.getSyncInfo("cfg", &none));
  EXPECT_FALSE(db.getSyncInfo("cfg", nullptr));
  seedSynced("cfg");
  JsonDocument before = info("cfg");
  EXPECT_EQ(before["list"].as<std::string>(), "cfg");
  EXPECT_FALSE(before["inst"].as<std::string>().empty());
  EXPECT_EQ(before["seq"].as<long>(), 3);
  EXPECT_EQ(before["cloudSeq"].as<long>(), -1);
  EXPECT_EQ(before["state"].as<std::string>(), "unknown");
  EXPECT_EQ(before["pending"].as<int>(), 3);
  EXPECT_EQ(before["restore"].as<int>(), -1);
  EXPECT_EQ(before["history"].as<std::string>(), "");
  EXPECT_FALSE(before["lost"].as<bool>());
  EXPECT_TRUE(before["syncEnabled"].as<bool>());
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 1)));
  JsonDocument after = info("cfg");
  EXPECT_EQ(after["cloudSeq"].as<long>(), 3);
  EXPECT_EQ(after["state"].as<std::string>(), "insync");
  EXPECT_EQ(after["pending"].as<int>(), 0);
}

// ------------------------------------------------------------ end to end ----

TEST_F(CloudSyncTest, EndToEndDeviceLifecycle) {
  // boot 1: offline writes
  ASSERT_TRUE(db.createList("telemetry"));
  ASSERT_TRUE(enableSync());
  ASSERT_TRUE(db.enableListSync("telemetry"));
  ASSERT_TRUE(add("telemetry", R"({"t":1,"temp":21.5})"));
  ASSERT_TRUE(add("telemetry", R"({"t":2,"temp":22.0})"));
  std::string id = inst("telemetry");
  // WiFi comes up: first sync, cloud knows nothing
  EXPECT_EQ(connectAndSync(), 1);
  ASSERT_TRUE(cloudSends(statusJson("telemetry", "", 0)));
  EXPECT_EQ(types(), (std::vector<std::string>{"query", "snapshot"}));
  EXPECT_EQ(message(1)["docs"].size(), 2u);
  // live changes flow as ops
  clearPublished();
  ASSERT_TRUE(add("telemetry", R"({"t":3,"temp":22.5})"));
  EXPECT_EQ(db.deleteRecords("telemetry", R"({"t":1})"), 1);
  EXPECT_EQ(types(), (std::vector<std::string>{"op", "op"}));
  // connection drops, writes continue
  mqtt.disconnect();
  clearPublished();
  ASSERT_TRUE(add("telemetry", R"({"t":4,"temp":23.0})"));
  EXPECT_EQ(publishedCount(), 0u);
  // reboot, reconnect: cloud reports it saw up to seq 4 (the delete)
  reboot();
  EXPECT_EQ(connectAndSync(), 1);
  ASSERT_TRUE(cloudSends(statusJson("telemetry", id, 4)));
  ASSERT_EQ(types(), (std::vector<std::string>{"query", "op"}));
  EXPECT_EQ(message(1)["seq"].as<long>(), 5);
  EXPECT_EQ(message(1)["doc"]["t"].as<int>(), 4);
  EXPECT_EQ(pending("telemetry"), 0);
  // flash wiped: device recreates the list empty and pulls the backup
  SPIFFS.reset();
  reboot();
  ASSERT_TRUE(db.createList("telemetry"));
  ASSERT_TRUE(db.enableListSync("telemetry"));
  EXPECT_NE(inst("telemetry"), id);
  mqtt.connect("esp-1");
  clearPublished();
  ASSERT_EQ(db.restoreFromCloud("telemetry"), 1);
  ASSERT_TRUE(cloudSends(snapshotJson("telemetry", id, 5, 0, true,
                                      R"([{"t":2,"temp":22.0},{"t":3,"temp":22.5},{"t":4,"temp":23.0}])")));
  EXPECT_EQ(db.getRecordCount("telemetry"), 3);
  EXPECT_EQ(inst("telemetry"), id);
  EXPECT_EQ(seq("telemetry"), 5);
  JsonDocument rows;
  ASSERT_EQ(db.getRecords("telemetry", R"({"t":4})", &rows), 1);
  EXPECT_DOUBLE_EQ(rows[0]["temp"].as<double>(), 23.0);
}

// ------------------------------------------------------- delta replay ----

// A minimal cloud store: applies snapshots and ops exactly as published,
// using the library's own filter rules for selectors. If its copy equals the
// device list after a mixed online/offline session, the delta stream is
// complete and correctly ordered.
namespace {

struct FakeCloud {
  std::vector<std::string> docs;
  std::string inst;
  long seq = 0;
  DBFilterProcessor filter;

  void apply(const std::string& payload) {
    JsonDocument msg;
    ASSERT_FALSE(deserializeJson(msg, payload)) << payload;
    std::string type = msg["type"].as<std::string>();
    if (type == "query" || type == "restore") return;
    if (type == "snapshot") {
      if (msg["i"].as<long>() == 0) docs.clear();
      for (JsonVariant doc : msg["docs"].as<JsonArray>()) docs.push_back(serialize(doc));
      if (msg["last"].as<bool>()) {
        inst = msg["inst"].as<std::string>();
        seq = msg["seq"].as<long>();
      }
      return;
    }
    ASSERT_EQ(type, "op");
    ASSERT_EQ(msg["inst"].as<std::string>(), inst);
    ASSERT_EQ(msg["seq"].as<long>(), seq + 1) << "out of order: " << payload;  // contiguous deltas
    seq = msg["seq"].as<long>();
    std::string op = msg["op"].as<std::string>();
    if (op == "add") {
      docs.push_back(serialize(msg["doc"]));
    } else if (op == "update" || op == "delete") {
      JsonObject selector = msg["sel"].as<JsonObject>();
      std::vector<std::string> next;
      int matched = 0;
      for (auto& stored : docs) {
        JsonDocument doc;
        deserializeJson(doc, stored);
        if (filter.filterParse(selector, doc.as<JsonVariant>(), false)) {
          next.push_back(stored);
          continue;
        }
        matched++;
        if (op == "update") next.push_back(serialize(msg["doc"]));
      }
      EXPECT_EQ(matched, msg["n"].as<int>()) << payload;
      docs = next;
    } else if (op == "drop") {
      docs.clear();
      inst.clear();
    }
  }
  static std::string serialize(JsonVariant v) {
    std::string out;
    serializeJson(v, out);
    return out;
  }
};

}  // namespace

TEST_F(CloudSyncTest, ReplayedDeltasReproduceTheDeviceList) {
  FakeCloud cloud;
  auto deviceDocs = [&] {
    std::vector<std::string> out;
    for (auto& line : jsonLines(SPIFFS.readRaw(listPath("cfg")))) {
      JsonDocument doc;
      deserializeJson(doc, line);
      out.push_back(FakeCloud::serialize(doc.as<JsonVariant>()));
    }
    return out;
  };
  auto drain = [&] {
    for (auto& m : mqtt.published()) cloud.apply(m.payload);
    clearPublished();
  };

  seedSynced("cfg");
  connectAndSync();
  drain();  // query: cloud has nothing
  ASSERT_TRUE(cloudSends(statusJson("cfg", cloud.inst, cloud.seq)));
  drain();  // snapshot
  EXPECT_EQ(cloud.docs, deviceDocs());

  // live session with every kind of change
  ASSERT_TRUE(add("cfg", R"({"name":"dave","age":24,"tags":["x"]})"));
  EXPECT_EQ(update("cfg", R"({"age":{"$gt":40}})", R"({"name":"senior","age":99})"), 2);
  EXPECT_EQ(db.deleteRecords("cfg", R"({"name":"alice"})"), 1);
  EXPECT_EQ(update("cfg", R"({"tags":{"$eleMatch":{"$eq":"x"}}})", R"({"name":"dave","age":25})"), 1);
  drain();
  EXPECT_EQ(cloud.docs, deviceDocs());

  // offline changes, then reconnect with the cloud reporting its position
  mqtt.disconnect();
  ASSERT_TRUE(add("cfg", R"({"name":"erin","age":36})"));
  EXPECT_EQ(db.deleteRecords("cfg", R"({"age":99})"), 2);
  EXPECT_EQ(publishedCount(), 0u);
  reboot();
  connectAndSync();
  drain();
  ASSERT_TRUE(cloudSends(statusJson("cfg", cloud.inst, cloud.seq)));
  drain();
  EXPECT_EQ(cloud.docs, deviceDocs());
  EXPECT_EQ(cloud.seq, seq("cfg"));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"dave", "erin"}));

  // oplog rolled over while offline: a snapshot brings the cloud back in line
  config.maxOplogBytes = 128;
  ASSERT_TRUE(enableSync());
  mqtt.disconnect();
  for (int i = 0; i < 8; i++) ASSERT_TRUE(add("cfg", ("{\"name\":\"bulk-" + std::to_string(i) + "\"}").c_str()));
  connectAndSync();
  drain();
  ASSERT_TRUE(cloudSends(statusJson("cfg", cloud.inst, cloud.seq)));
  EXPECT_EQ(types().back(), "snapshot");
  drain();
  EXPECT_EQ(cloud.docs, deviceDocs());
  EXPECT_EQ(cloud.seq, seq("cfg"));

  // finally the list is dropped
  ASSERT_TRUE(db.deleteList("cfg"));
  drain();
  EXPECT_TRUE(cloud.docs.empty());
  EXPECT_TRUE(cloud.inst.empty());
}

// -------------------------------------------------------- integrity ----

TEST(CloudChecksum, Crc32MatchesZlib) {
  // zlib.crc32(b"123456789") == 0xCBF43926
  EXPECT_EQ(DBUtils::crc32(0, "123456789", 9), 0xCBF43926UL);
  EXPECT_EQ(DBUtils::crc32(0, "", 0), 0UL);
  // chaining over blocks gives the same result as one pass
  unsigned long part = DBUtils::crc32(0, "1234", 4);
  EXPECT_EQ(DBUtils::crc32(part, "56789", 5), 0xCBF43926UL);
  EXPECT_EQ(DBUtils::crc32(0, "{\"a\":1}\n", 8), DBUtils::crc32(DBUtils::crc32(0, "{\"a\":1}", 7), "\n", 1));
}

TEST_F(CloudSyncTest, RestoreIsInstalledOnlyWhenTheChecksumMatches) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  std::string before = SPIFFS.readRaw(listPath("cfg"));
  // wrong crc: staged copy discarded, local list untouched, sync state reset
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 9, 0, false, R"([{"name":"c1"},{"name":"c2"}])")));
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 1, true, R"([{"name":"c3"}])", /*crcOverride=*/12345)));
  EXPECT_EQ(SPIFFS.readRaw(listPath("cfg")), before);
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
  EXPECT_FALSE(SPIFFS.exists(newPath("cfg").c_str()));
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_NE(inst("cfg"), "x");
  EXPECT_EQ(seq("cfg"), 3);
  // wrong count: same
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 0, true, R"([{"name":"c1"}])", -1, /*countOverride=*/2)));
  EXPECT_EQ(SPIFFS.readRaw(listPath("cfg")), before);
  // no checksum at all: integrity cannot be verified, discarded
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 0, true, R"([{"name":"c1"}])", -1, -1, /*withChecksum=*/false)));
  EXPECT_EQ(SPIFFS.readRaw(listPath("cfg")), before);
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));
  // matching checksum over three chunks: installed
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 9, 0, false, R"([{"name":"c1","v":1.5}])")));
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 9, 1, false, R"([{"name":"c2"},{"name":"c3","tags":["a","b"]}])")));
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 9, 2, true, R"([{"name":"c4"}])")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"c1", "c2", "c3", "c4"}));
  EXPECT_EQ(inst("cfg"), "x");
  EXPECT_EQ(seq("cfg"), 9);
  EXPECT_EQ(state("cfg"), "insync");
}

TEST_F(CloudSyncTest, ChecksumCoversCompactDocumentsNotWireFormatting) {
  // The cloud may send the documents with whitespace; the checksum is over
  // the compact form that ends up in the list file
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  unsigned long crc = crcOfDocs({"{\"name\":\"c1\",\"n\":2}"});
  std::string chunk = "{\"type\":\"snapshot\", \"list\":\"cfg\", \"inst\":\"x\", \"seq\":1, \"i\":0, \"last\":true, \"docs\":[ { \"name\" : \"c1\", \"n\" : 2 } ],"
                      " \"crc\":" + std::to_string(crc) + ", \"count\":1, \"mid\":" + std::to_string(info("cfg")["restoreMid"].as<unsigned long>()) + "}";
  EXPECT_TRUE(cloudSends(chunk));
  EXPECT_EQ(SPIFFS.readRaw(listPath("cfg")), "{\"name\":\"c1\",\"n\":2}\r\n");
}

TEST_F(CloudSyncTest, OutboundSnapshotCarriesChecksumAndCountOnLastChunk) {
  config.maxPayloadBytes = 220;
  seedSynced("cfg");
  for (int i = 0; i < 6; i++) ASSERT_TRUE(add("cfg", ("{\"name\":\"padding-padding-" + std::to_string(i) + "\",\"v\":" + std::to_string(i) + "}").c_str()));
  connectAndSync();
  clearPublished();
  ASSERT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  ASSERT_GT(publishedCount(), 1u);
  std::vector<std::string> docs;
  for (size_t i = 0; i < publishedCount(); i++) {
    JsonDocument chunk = message(i);
    bool last = i + 1 == publishedCount();
    EXPECT_EQ(chunk["crc"].is<unsigned long>(), last) << payload(i);
    EXPECT_EQ(chunk["count"].is<long>(), last) << payload(i);
    for (JsonVariant v : chunk["docs"].as<JsonArray>()) {
      std::string d;
      serializeJson(v, d);
      docs.push_back(d);
    }
    if (last) {
      EXPECT_EQ(chunk["count"].as<long>(), 9);
      EXPECT_EQ(chunk["crc"].as<unsigned long>(), crcOfDocs(docs));
      EXPECT_LE(payload(i).size(), 220u);
    }
  }
  EXPECT_EQ(docs.size(), 9u);
}

// ------------------------------------------------------ rogue cloud ----
// A misbehaving cloud must not be able to make the device publish, read
// flash or stage data beyond what the device itself asked for.

TEST_F(CloudSyncTest, UnsolicitedStatusNeverCausesTraffic) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  EXPECT_EQ(state("cfg"), "insync");
  clearPublished();
  // not waiting for an answer: every one of these would have meant a snapshot
  for (int i = 0; i < 20; i++) {
    cloudSends(statusJson("cfg", "", 0));
    cloudSends(statusJson("cfg", "stranger", 99));
    cloudSends(statusJson("cfg", inst("cfg"), 0));
  }
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "insync");
  EXPECT_EQ(cloudSeq("cfg"), 3);  // an ack never moves backwards either
  EXPECT_EQ(db.getRecordCount("cfg"), 3);
}

TEST_F(CloudSyncTest, OneQueryGetsAtMostOneAnswerActedOn) {
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  ASSERT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  clearPublished();
  // a second answer to the same query is just information
  for (int i = 0; i < 10; i++) cloudSends(statusJson("cfg", "", 0));
  EXPECT_EQ(publishedCount(), 0u);
  // until the device asks again
  db.syncCloud();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot"}));
}

TEST_F(CloudSyncTest, UnsolicitedStatusDoesNotMakeAQueriedListSkipItsAnswer) {
  // The only status acted on is the one answering the open query, and it
  // is acted on once: a tombstone or restore cannot be triggered twice.
  config.restoreOnMismatch = true;
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  EXPECT_TRUE(cloudSends(statusJson("cfg", "other", 5)));
  EXPECT_EQ(types(), (std::vector<std::string>{"restore"}));
  clearPublished();
  for (int i = 0; i < 5; i++) cloudSends(statusJson("cfg", "other", 5));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "restoring");
}

TEST_F(CloudSyncTest, RequestsAreThrottledPerList) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  clearPublished();
  std::string req = "{\"type\":\"request\",\"list\":\"cfg\",\"inst\":\"" + inst("cfg") + "\",\"from\":1,\"to\":3}";
  EXPECT_TRUE(cloudSends(req));
  EXPECT_EQ(publishedCount(), 3u);
  clearPublished();
  for (int i = 0; i < 50; i++) EXPECT_FALSE(cloudSends(req));
  EXPECT_EQ(publishedCount(), 0u);
  advanceMillis(config.minCloudRequestIntervalMs);
  EXPECT_TRUE(cloudSends(req));
  EXPECT_EQ(publishedCount(), 3u);
  // requests during a restore are ignored as well
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  clearPublished();
  advanceMillis(config.minCloudRequestIntervalMs);
  EXPECT_FALSE(cloudSends(req));
  EXPECT_EQ(publishedCount(), 0u);
}

TEST_F(CloudSyncTest, OversizedInboundMessageIsDroppedBeforeParsing) {
  config.maxInboundBytes = 256;
  seedSynced("cfg");
  connectAndSync();
  clearPublished();
  std::string padded = "{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"\",\"seq\":0,\"pad\":\"" + std::string(300, 'x') + "\"}";
  EXPECT_FALSE(cloudSends(padded));
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "queried");  // still waiting for a proper answer
  EXPECT_TRUE(cloudSends(statusJson("cfg", "", 0)));
  EXPECT_EQ(types(), (std::vector<std::string>{"snapshot"}));
  QuarkSyncConfig tiny = config;
  tiny.maxInboundBytes = 10;
  EXPECT_FALSE(db.enableSync(&transport, tiny));
}

TEST_F(CloudSyncTest, SnapshotsWhenNotRestoringTouchNothing) {
  seedSynced("cfg");
  connectAndSync();
  ASSERT_TRUE(cloudSends(statusJson("cfg", inst("cfg"), 3)));
  std::string before = SPIFFS.readRaw(listPath("cfg"));
  for (int i = 0; i < 20; i++) {
    EXPECT_FALSE(cloudSends(snapshotJson("cfg", inst("cfg"), 3, i, i == 19, R"([{"name":"evil"}])")));
  }
  EXPECT_EQ(SPIFFS.readRaw(listPath("cfg")), before);
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
  EXPECT_EQ(inst("cfg"), inst("cfg"));
}

TEST_F(CloudSyncTest, RestoreIsAbortedWhenTooMuchDataArrives) {
  config.maxRestoreBytes = 200;
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  std::string doc = "{\"blob\":\"" + std::string(60, 'x') + "\"}";
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 9, 0, false, "[" + doc + "," + doc + "]")));   // ~150 bytes staged
  EXPECT_TRUE(SPIFFS.exists(restorePath("cfg").c_str()));
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 1, false, "[" + doc + "]")));            // over 200
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
  // nothing more is accepted, the local list is intact
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 2, true, "[" + doc + "]")));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol"}));
  EXPECT_EQ(inst("cfg"), inst("cfg"));
}

TEST_F(CloudSyncTest, RestoreWhoseChunksStopComingIsAbandoned) {
  config.restoreTimeoutMs = 60000;
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  EXPECT_TRUE(cloudSends(snapshotJson("cfg", "x", 9, 0, false, R"([{"name":"c1"}])")));
  advanceMillis(30000);
  clearPublished();
  ASSERT_TRUE(add("cfg", R"({"name":"meanwhile"})"));  // still waiting: logged only
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "restoring");
  advanceMillis(30000);
  // the late chunk is refused, the staging file is gone and the list goes on as master
  EXPECT_FALSE(cloudSends(snapshotJson("cfg", "x", 9, 1, true, R"([{"name":"c2"}])")));
  EXPECT_EQ(state("cfg"), "unknown");
  EXPECT_FALSE(SPIFFS.exists(restorePath("cfg").c_str()));
  EXPECT_EQ(findNames("cfg", "{}"), (Names{"alice", "bob", "carol", "meanwhile"}));
  ASSERT_TRUE(add("cfg", R"({"name":"later"})"));
  EXPECT_EQ(types(), (std::vector<std::string>{"query"}));  // back to the normal handshake
  // syncCloud() does not re-request a restore on its own either
  db.syncCloud();
  for (auto& t : types()) EXPECT_NE(t, "restore");
}

TEST_F(CloudSyncTest, SyncCloudDoesNotInterruptAPendingRestore) {
  seedSynced("cfg");
  mqtt.connect("esp-1");
  ASSERT_EQ(db.restoreFromCloud("cfg"), 1);
  clearPublished();
  EXPECT_EQ(db.syncCloud(), 0);  // the list keeps waiting for its chunks, no new query
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(state("cfg"), "restoring");
}

TEST_F(CloudSyncTest, NoMessageIsEverSentWithoutALocalCause) {
  // A long stream of garbage and unsolicited messages on the subscribe
  // topic yields zero publishes and no flash files beyond the list's own.
  seedSynced("cfg");
  mqtt.connect("esp-1");  // no syncCloud(): the device asked for nothing
  size_t files = SPIFFS.paths().size();
  const char* junk[] = {
      "{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"\",\"seq\":0}",
      "{\"type\":\"status\",\"list\":\"cfg\",\"inst\":\"ghost\",\"seq\":1000}",
      "{\"type\":\"snapshot\",\"list\":\"cfg\",\"inst\":\"ghost\",\"seq\":1,\"i\":0,\"last\":true,\"docs\":[{\"a\":1}]}",
      "{\"type\":\"restore\",\"list\":\"cfg\"}",
      "{\"type\":\"query\",\"list\":\"cfg\"}",
      "{\"type\":\"op\",\"list\":\"cfg\",\"op\":\"drop\"}",
      "{\"type\":\"ack\",\"list\":\"cfg\",\"inst\":\"ghost\",\"seq\":5}",
      "{\"type\":\"request\",\"list\":\"cfg\",\"inst\":\"ghost\",\"from\":1,\"to\":3}",
      "[]", "null", "42", "{", "",
  };
  for (int round = 0; round < 10; round++) {
    for (const char* m : junk) cloudSends(m);
  }
  EXPECT_EQ(publishedCount(), 0u);
  EXPECT_EQ(SPIFFS.paths().size(), files);
  EXPECT_EQ(db.getRecordCount("cfg"), 3);
  EXPECT_EQ(state("cfg"), "unknown");
}
