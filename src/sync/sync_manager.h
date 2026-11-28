// The MIT License (MIT)
//
// Copyright (c) 2024 Quarkifi Technologies Pvt Ltd
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

// Cloud backup protocol. The device is the master copy; the cloud keeps a
// snapshot of each synced list and applies the deltas the device publishes.
//
// Every message is a JSON object on a single topic pair and carries "type",
// "list" and "inst" (the list instance id), plus "dev" when a device id is
// configured. Every message the device publishes also carries a unique
// numeric message id "mid"; the cloud correlates its answers to it.
//
// Device -> cloud (publishTopic)
//   {"type":"query","list":"cfg","inst":"a1","seq":12,"mid":101}
//       asks what the cloud holds; sent by syncCloud() and before the first
//       push after a change. "inst" is "" for a deleted list (tombstone)
//   {"type":"op","list":"cfg","inst":"a1","seq":13,"mid":102,"op":"add","doc":{..}}
//       one oplog entry ("add", "update" with "sel"+"doc"+"n", "delete"
//       with "sel"+"n", or "drop" once the list was deleted)
//   {"type":"snapshot","list":"cfg","inst":"a1","seq":13,"mid":103,"i":0,"last":true,"docs":[..],"crc":3421780262,"count":7}
//       full copy in chunks of at most maxPayloadBytes; chunk 0 replaces
//       the cloud copy, "last" commits it at "seq". The last chunk carries
//       the integrity check of the whole snapshot: "count" documents and
//       "crc", the CRC-32 (zlib) of every document in compact JSON form
//       followed by "\n", in order
//   {"type":"restore","list":"cfg","inst":"a1","seq":13,"mid":104}
//       asks the cloud to send its copy as "snapshot" chunks
//
// Cloud -> device (subscribeTopic)
//   {"type":"status","list":"cfg","inst":"a1","seq":10,"mid":101}
//       answer to the query with that "mid" (authoritative), inst "" means
//       no copy. Only acted on while the device is waiting for exactly that
//       answer (state QUERIED and "mid" equal to the open query); a status
//       for an earlier query, or an unsolicited one, is ignored. The cloud
//       instance is classified against what the device remembers for the
//       list name:
//         current id          -> push the missing ops, or a snapshot when
//                                they rolled over; if the local file was
//                                lost and created again, pull the cloud
//                                copy when the restore option allows it
//         id in the history   -> the device deleted it on purpose: never
//                                restore, push a snapshot ("drop" if the
//                                list no longer exists)
//         unknown id          -> the device has no memory of it (wiped):
//                                pull when allowed, otherwise push a snapshot
//         no copy             -> push a snapshot
//   {"type":"ack","list":"cfg","mid":102}
//       acknowledges the "op" or final "snapshot" chunk published with that
//       "mid". The device maps it back to the seq it carried and moves the
//       known cloud position forward, never backwards, so acks may arrive
//       in any order. Unknown mids are ignored; never triggers a resend
//   {"type":"request","list":"cfg","inst":"a1","from":34,"to":45}
//       the cloud found a gap and asks for a range: the device replays from
//       "from" or sends a snapshot when its oplog no longer starts that far
//       back. Served at most once per minCloudRequestIntervalMs per list
//   {"type":"snapshot",...,"mid":104,...,"crc":..,"count":..}   same shape
//       as above, answer to "restore" and carrying its "mid" in every chunk;
//       only accepted while that restore is pending, in order, up to
//       maxRestoreBytes and within restoreTimeoutMs. The staged copy only
//       replaces the local list if the "crc" and "count" of the last chunk
//       match what was staged; otherwise the restore is discarded
//
// Defensive rules: nothing here loops or retries by itself. Every outbound
// message is caused by a local change, by syncCloud(), or by exactly one
// inbound message the device asked for (or a throttled "request"). Inbound
// payloads above maxInboundBytes are dropped before parsing.
#ifndef QUARK_DB_SYNC_MANAGER_H
#define QUARK_DB_SYNC_MANAGER_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include "../QuarkTransport.h"
#include "../utils/db_utils.h"
#include "../writer/write_listener.h"
#include "sync_config.h"
#include "oplog.h"

class DBSyncManager : public DBWriteListener {
  private:
    DBOplog* oplog;
    DBUtils* dbUtils;
    QuarkTransport* transport;
    QuarkSyncConfig config;
    bool enabled;
    unsigned long nextMid;
    unsigned long newMid();
    bool online();
    size_t payloadLimit();
    bool publish(const String& payload);
    String header(const char* type, const String& listName, const String& inst, unsigned long mid);
    bool sendQuery(SyncListState* state);
    bool sendRestore(SyncListState* state);
    bool sendDrop(SyncListState* state, const String& inst);
    int pushPending(SyncListState* state);
    int pushSnapshot(SyncListState* state);
    bool restoreAllowed(SyncListState* state);
    bool handleStatus(SyncListState* state, JsonObject msg);
    bool handleRequest(SyncListState* state, JsonObject msg);
    bool handleAck(SyncListState* state, JsonObject msg);
    bool handleSnapshot(SyncListState* state, JsonObject msg);
    void abortRestore(SyncListState* state);
    void checkRestoreTimeout(SyncListState* state);
    bool settled(SyncListState* state);
    void afterLocalChange(const String& listName, long seq);
  public:
    DBSyncManager();
    ~DBSyncManager();
    void init(DBOplog* oplog);
    bool enableSync(QuarkTransport* transport, const QuarkSyncConfig& config);
    void disableSync();
    bool isSyncEnabled();
    bool enableListSync(String listName, QuarkListSyncOptions options);
    bool disableListSync(String listName);
    bool isListSyncEnabled(String listName);
    int syncCloud();
    bool isQuarkDBMessage(const char* topic, const String& payload);
    bool handleMessage(const char* topic, const String& payload);
    int restoreFromCloud(String listName);
    bool getSyncInfo(String listName, JsonDocument* info);
    // MQTT topic filter match with "+" and "#" wildcards
    static bool topicMatches(const char* filter, const char* topic);
    // DBWriteListener
    void onRecordAdded(const String& listName, const String& docJson);
    void onRecordsUpdated(const String& listName, const String& selectorJson, const String& docJson, int count);
    void onRecordsDeleted(const String& listName, const String& selectorJson, int count);
    void onListDeleted(const String& listName);
    void onListCreated(const String& listName);
};

#endif
