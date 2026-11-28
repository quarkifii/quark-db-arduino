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

// Cloud backup protocol implementation, see sync_manager.h for the messages
#include "sync_manager.h"
#include <ArduinoJson.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif

// Escapes a string for use inside a JSON string literal
static String jsonEscape(const String& in) {
  String out;
  for (unsigned int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c == '"' || c == '\\') {
      out += '\\';
      out += c;
    } else if (c == '\n') {
      out += "\\n";
    } else if (c == '\r') {
      out += "\\r";
    } else if (c == '\t') {
      out += "\\t";
    } else if ((unsigned char)c < 0x20) {
      // other control characters are dropped
    } else {
      out += c;
    }
  }
  return out;
}

DBSyncManager::DBSyncManager() {
  oplog = NULL;
  dbUtils = new DBUtils();
  transport = NULL;
  enabled = false;
  nextMid = 0;
}

// Message ids start at a random value so answers to messages of an earlier
// boot can never match, and never hand out 0 (reserved for "none")
unsigned long DBSyncManager::newMid() {
  if (nextMid == 0) {
    nextMid = dbUtils->randomValue();
  }
  nextMid++;
  if (nextMid == 0) {
    nextMid = 1;
  }
  return nextMid;
}

DBSyncManager::~DBSyncManager() {
  delete dbUtils;
}

void DBSyncManager::init(DBOplog* oplog) {
  this->oplog = oplog;
}

bool DBSyncManager::enableSync(QuarkTransport* transport, const QuarkSyncConfig& config) {
  if (transport == NULL) {
    Serial.println("ERROR: Sync transport is NULL");
    return false;
  }
  if (config.publishTopic.length() == 0 || config.subscribeTopic.length() == 0) {
    Serial.println("ERROR: Sync topics must not be empty");
    return false;
  }
  if (config.maxPayloadBytes < 64 || config.maxOplogBytes < 64 || config.maxInboundBytes < 64) {
    Serial.println("ERROR: Sync byte limits too small");
    return false;
  }
  this->transport = transport;
  this->config = config;
  this->enabled = true;
  if (oplog != NULL) {
    oplog->setMaxBytes(config.maxOplogBytes);
  }
  return true;
}

void DBSyncManager::disableSync() {
  enabled = false;
  transport = NULL;
}

bool DBSyncManager::isSyncEnabled() {
  return enabled;
}

bool DBSyncManager::online() {
  return enabled && transport != NULL && transport->connected();
}

// Smaller of the configured payload size and what the transport can carry
size_t DBSyncManager::payloadLimit() {
  size_t limit = config.maxPayloadBytes;
  if (transport != NULL) {
    size_t transportLimit = transport->maxPayloadSize(config.publishTopic.c_str());
    if (transportLimit > 0 && transportLimit < limit) {
      limit = transportLimit;
    }
  }
  return limit;
}

bool DBSyncManager::publish(const String& payload) {
  if (!online()) {
    return false;
  }
  if (payload.length() > payloadLimit()) {
    Serial.println("ERROR: Sync message larger than max payload bytes (" + String((unsigned long)payload.length()) + ")");
    return false;
  }
  if (!transport->publish(config.publishTopic.c_str(), payload.c_str())) {
    Serial.println("ERROR: Sync publish failed");
    return false;
  }
  return true;
}

// Start of every message; the caller adds its fields and the closing brace
String DBSyncManager::header(const char* type, const String& listName, const String& inst, unsigned long mid) {
  String head = "{\"__quarkdb__msg__\":true,\"type\":\"" + String(type) + "\",\"list\":\"" + listName + "\"";
  if (config.deviceId.length() > 0) {
    head += ",\"dev\":\"" + jsonEscape(config.deviceId) + "\"";
  }
  head += ",\"inst\":\"" + inst + "\",\"mid\":" + String(mid);
  return head;
}

bool DBSyncManager::restoreAllowed(SyncListState* s) {
  if (s->restore == QUARK_RESTORE_INHERIT) {
    return config.restoreOnMismatch;
  }
  return s->restore == QUARK_RESTORE_ON_MISMATCH;
}

// ------------------------------------------------------------ list API ----

bool DBSyncManager::enableListSync(String listName, QuarkListSyncOptions options) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    Serial.println("ERROR:List Name Invalid ->" + listName);
    return false;
  }
  dbUtils->recoverList(listName);
  if (!SPIFFS.exists(dbUtils->getListFileName(listName))) {
    Serial.println("ERROR:List does not exist ->" + listName);
    return false;
  }
  return oplog->enable(listName, options.restore);
}

bool DBSyncManager::disableListSync(String listName) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return false;
  }
  return oplog->disable(listName);
}

bool DBSyncManager::isListSyncEnabled(String listName) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return false;
  }
  return oplog->isEnabled(listName);
}

bool DBSyncManager::getSyncInfo(String listName, JsonDocument* info) {
  listName.trim();
  if (info == NULL || !dbUtils->validateListName(listName.c_str())) {
    return false;
  }
  SyncListState* s = oplog->get(listName);
  if (s == NULL) {
    return false;
  }
  static const char* stateNames[] = {"unknown", "queried", "insync", "restoring"};
  (*info)["list"] = s->name;
  (*info)["inst"] = s->inst;
  (*info)["seq"] = s->seq;
  (*info)["cloudSeq"] = s->cloudSeq;
  (*info)["state"] = stateNames[s->state < 4 ? s->state : 0];
  (*info)["pending"] = oplog->countAfter(s->name, s->cloudSeq < 0 ? 0 : s->cloudSeq);
  (*info)["restore"] = (int)s->restore;
  (*info)["history"] = s->history;
  (*info)["lost"] = s->lost;
  (*info)["queryMid"] = s->queryMid;
  (*info)["restoreMid"] = s->restoreMid;
  (*info)["syncEnabled"] = enabled;
  return true;
}

// ------------------------------------------------------ local changes ----

void DBSyncManager::onRecordAdded(const String& listName, const String& docJson) {
  if (!oplog->isEnabled(listName)) {
    return;
  }
  afterLocalChange(listName, oplog->append(listName, "add", NULL, &docJson, -1));
}

void DBSyncManager::onRecordsUpdated(const String& listName, const String& selectorJson, const String& docJson, int count) {
  if (!oplog->isEnabled(listName)) {
    return;
  }
  afterLocalChange(listName, oplog->append(listName, "update", &selectorJson, &docJson, count));
}

void DBSyncManager::onRecordsDeleted(const String& listName, const String& selectorJson, int count) {
  if (!oplog->isEnabled(listName)) {
    return;
  }
  afterLocalChange(listName, oplog->append(listName, "delete", &selectorJson, NULL, count));
}

// The list file is gone. The metadata becomes a tombstone and the instance
// joins the history, so the cloud copy can never be restored by mistake.
// Online, the cloud is told right away; offline, the next syncCloud() asks
// the cloud what it holds and drops it then.
void DBSyncManager::onListDeleted(const String& listName) {
  if (!oplog->isEnabled(listName)) {
    return;
  }
  SyncListState* s = oplog->get(listName);
  String inst = s->inst;
  if (!oplog->markDropped(listName)) {
    return;
  }
  if (online()) {
    sendDrop(s, inst);
  }
}

// A list created while its metadata still holds a live instance was lost
// (not deleted through the API): the cloud copy may be the better one
void DBSyncManager::onListCreated(const String& listName) {
  SyncListState* s = oplog->get(listName);
  if (s == NULL || s->inst.length() == 0) {
    return;
  }
  Serial.println("Sync: list was lost and created again ->" + listName);
  oplog->markLost(listName);
}

// Push the new entry right away when the cloud state is known, otherwise
// find out what the cloud has first. Offline changes just accumulate.
void DBSyncManager::afterLocalChange(const String& listName, long seq) {
  if (seq < 0 || !online()) {
    return;
  }
  SyncListState* s = oplog->get(listName);
  if (s == NULL) {
    return;
  }
  checkRestoreTimeout(s);
  if (s->state == QUARK_SYNC_IN_SYNC) {
    pushPending(s);
  } else if (s->state == QUARK_SYNC_UNKNOWN) {
    sendQuery(s);
  } else if (s->state == QUARK_SYNC_QUERIED && config.queryTimeoutMs > 0 &&
             millis() - s->queriedAt >= config.queryTimeoutMs) {
    // The answer to the last query never came: ask once more (one message
    // per timeout period, only when there is something new to sync)
    Serial.println("Sync: no answer from cloud, querying again ->" + listName);
    sendQuery(s);
  }
  // QUERIED within the timeout or RESTORING: the entry waits in the oplog
}

// A restore whose chunks stopped coming is abandoned; nothing is retried
void DBSyncManager::checkRestoreTimeout(SyncListState* s) {
  if (s->state == QUARK_SYNC_RESTORING && config.restoreTimeoutMs > 0 &&
      millis() - s->restoreStartedAt >= config.restoreTimeoutMs) {
    Serial.println("Sync: restore timed out ->" + s->name);
    abortRestore(s);
  }
}

// The cloud is known to hold the current instance: the history of deleted
// ones and the lost flag are moot
bool DBSyncManager::settled(SyncListState* s) {
  return oplog->clearHistory(s);
}

// ------------------------------------------------------------ sending ----

bool DBSyncManager::sendQuery(SyncListState* s) {
  unsigned long mid = newMid();
  String msg = header("query", s->name, s->inst, mid) + ",\"seq\":" + String(s->seq) + "}";
  if (!publish(msg)) {
    s->state = QUARK_SYNC_UNKNOWN;
    return false;
  }
  s->state = QUARK_SYNC_QUERIED;
  s->queriedAt = millis();
  s->queryMid = mid;  // only the answer to this query counts
  return true;
}

bool DBSyncManager::sendRestore(SyncListState* s) {
  s->state = QUARK_SYNC_RESTORING;
  s->restoreStartedAt = millis();
  s->restoreBytes = 0;
  s->restoreCrc = 0;
  s->restoreCount = 0;
  s->restoreChunk = 0;
  s->restoreInst = "";
  s->restoreSeq = 0;
  unsigned long mid = newMid();
  String msg = header("restore", s->name, s->inst, mid) + ",\"seq\":" + String(s->seq) + "}";
  if (!publish(msg)) {
    s->state = QUARK_SYNC_UNKNOWN;
    return false;
  }
  s->restoreMid = mid;  // every chunk must carry it
  return true;
}

// Tells the cloud the instance it holds was deleted. The tombstone is
// removed once the message went out.
bool DBSyncManager::sendDrop(SyncListState* s, const String& inst) {
  String msg = header("op", s->name, inst, newMid()) + ",\"seq\":" + String(s->seq) + ",\"op\":\"drop\"}";
  if (!publish(msg)) {
    return false;
  }
  Serial.println("Sync: dropped list reported ->" + s->name);
  return oplog->disable(s->name);
}

// Publishes every oplog entry the cloud does not have yet, in order. A
// missing sequence number (rollover, oversized entry) or an entry that does
// not fit in a message means the log cannot bring the cloud up to date, so
// the current snapshot is sent instead. Returns the number of messages
// published, or -1 if one could not be sent.
int DBSyncManager::pushPending(SyncListState* s) {
  if (s->cloudSeq < 0) {
    return -1;
  }
  if (s->cloudSeq >= s->seq) {
    return 0;
  }
  long expected = s->cloudSeq + 1;
  int sent = 0;
  bool gap = false;
  size_t limit = payloadLimit();
  File file = oplog->openLog(s->name);
  if (file) {
    while (file.available()) {
      String line = file.readStringUntil('\n');
      line.trim();
      long seq = DBOplog::parseSeq(line);
      if (seq < 0 || seq < expected) {
        continue;
      }
      if (seq > expected) {
        gap = true;
        break;
      }
      // A torn line (power loss mid-append) cannot be replayed
      JsonDocument check;
      if (deserializeJson(check, line)) {
        gap = true;
        break;
      }
      unsigned long mid = newMid();
      String msg = header("op", s->name, s->inst, mid) + "," + line.substring(1);
      if (msg.length() > limit) {
        gap = true;
        break;
      }
      if (!publish(msg)) {
        file.close();
        return -1;
      }
      DBOplog::rememberSent(s, mid, seq);
      s->cloudSeq = seq;
      expected++;
      sent++;
    }
    file.close();
  }
  if (!gap && expected <= s->seq) {
    gap = true;
  }
  if (gap) {
    Serial.println("Sync: oplog incomplete, sending snapshot ->" + s->name);
    if (pushSnapshot(s) < 0) {
      return -1;
    }
    sent++;
  }
  return sent;
}

// Publishes the whole list as snapshot chunks that fit the payload limit.
// Returns the number of chunks, or -1 if one could not be sent.
int DBSyncManager::pushSnapshot(SyncListState* s) {
  dbUtils->recoverList(s->name);
  String fileName = dbUtils->getListFileName(s->name);
  if (!SPIFFS.exists(fileName)) {
    Serial.println("ERROR: Sync list missing ->" + s->name);
    return -1;
  }
  File file = SPIFFS.open(fileName, "r");
  if (!file) {
    Serial.println("ERROR: Sync could not read list ->" + s->name);
    return -1;
  }
  size_t limit = payloadLimit();
  // Each chunk gets its own message id; the header size does not depend on it
  String tail = ",\"seq\":" + String(s->seq) + ",\"i\":";
  size_t headLength = header("snapshot", s->name, s->inst, 0).length() + 10 + tail.length();
  // room for the chunk index, ",\"last\":false,\"docs\":[", "]}" and the
  // ",\"crc\":N,\"count\":N" of the last chunk
  size_t overhead = headLength + 10 + 23 + 2 + 32;
  long chunk = 0;
  unsigned long mid = 0;
  unsigned long crc = 0;
  long count = 0;
  String docs;
  bool ok = true;
  while (ok && file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line == "") {
      continue;
    }
    // Same rule as reads: partial or corrupt lines are not records
    JsonDocument check;
    if (deserializeJson(check, line)) {
      continue;
    }
    if (docs.length() > 0 && overhead + docs.length() + 1 + line.length() > limit) {
      mid = newMid();
      ok = publish(header("snapshot", s->name, s->inst, mid) + tail + String(chunk) + ",\"last\":false,\"docs\":[" + docs + "]}");
      chunk++;
      docs = "";
    }
    if (docs.length() > 0) {
      docs += ",";
    }
    docs += line;
    // Integrity check the cloud verifies: every document plus "\n"
    crc = DBUtils::crc32(crc, line.c_str(), line.length());
    crc = DBUtils::crc32(crc, "\n", 1);
    count++;
  }
  file.close();
  if (ok) {
    mid = newMid();
    ok = publish(header("snapshot", s->name, s->inst, mid) + tail + String(chunk) + ",\"last\":true,\"docs\":[" + docs +
                 "],\"crc\":" + String(crc) + ",\"count\":" + String(count) + "}");
  }
  if (!ok) {
    s->state = QUARK_SYNC_UNKNOWN;
    return -1;
  }
  DBOplog::rememberSent(s, mid, s->seq);  // the last chunk commits the snapshot
  s->cloudSeq = s->seq;
  s->state = QUARK_SYNC_IN_SYNC;
  settled(s);
  Serial.println("Sync: snapshot sent ->" + s->name + " (" + String(chunk + 1) + " chunks)");
  return (int)chunk + 1;
}

// ---------------------------------------------------------- public API ----

// Subscribes and asks the cloud what it holds for every synced list,
// including tombstones of deleted lists (the answer tells which deleted
// instance to drop). Returns the number of lists queried, -1 if sync is
// not enabled or the transport is offline.
int DBSyncManager::syncCloud() {
  if (!online()) {
    return -1;
  }
  if (!transport->subscribe(config.subscribeTopic.c_str())) {
    Serial.println("ERROR: Sync subscribe failed ->" + config.subscribeTopic);
    return -1;
  }
  oplog->loadAll();
  int queried = 0;
  for (int i = 0; i < oplog->size(); i++) {
    SyncListState* s = oplog->at(i);
    if (s == NULL) {
      continue;
    }
    if (s->inst.length() == 0 && s->history.length() == 0) {
      continue;  // nothing to tell or ask
    }
    if (s->state == QUARK_SYNC_RESTORING) {
      checkRestoreTimeout(s);
      if (s->state == QUARK_SYNC_RESTORING) {
        continue;  // keep waiting for the chunks already requested
      }
    }
    if (sendQuery(s)) {
      queried++;
    }
  }
  return queried;
}

// On demand pull. Returns 1 when the request went out, 0 when offline or
// the publish failed, -1 when the list is not synced.
int DBSyncManager::restoreFromCloud(String listName) {
  listName.trim();
  if (!enabled || !dbUtils->validateListName(listName.c_str())) {
    return -1;
  }
  SyncListState* s = oplog->get(listName);
  if (s == NULL || s->inst.length() == 0) {
    return -1;
  }
  if (!online()) {
    return 0;
  }
  return sendRestore(s) ? 1 : 0;
}

bool DBSyncManager::isQuarkDBMessage(const char* topic, const String& payload) {
  (void)topic;
  if (payload.length() == 0) {
    return false;
  }
  if (config.maxInboundBytes > 0 && payload.length() > config.maxInboundBytes) {
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    return false;
  }
  JsonObject msg = doc.as<JsonObject>();
  if (msg.isNull()) {
    return false;
  }
  return msg["__quarkdb__msg__"].is<bool>() && msg["__quarkdb__msg__"].as<bool>();
}

bool DBSyncManager::handleMessage(const char* topic, const String& payload) {
  if (!enabled || topic == NULL || !topicMatches(config.subscribeTopic.c_str(), topic)) {
    return false;
  }
  if (payload.length() > config.maxInboundBytes) {
    Serial.println("ERROR: Sync message larger than max inbound bytes, dropped");
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, payload)) {
    Serial.println("ERROR: Sync message is not valid JSON");
    return false;
  }
  JsonObject msg = doc.as<JsonObject>();
  if (msg.isNull()) {
    return false;
  }
  String type = msg["type"] | "";
  String listName = msg["list"] | "";
  if (config.deviceId.length() > 0 && msg["dev"].is<const char*>() && config.deviceId != String(msg["dev"] | "")) {
    return false;  // addressed to another device
  }
  if (listName.length() == 0 || !dbUtils->validateListName(listName.c_str())) {
    return false;
  }
  SyncListState* s = oplog->get(listName);
  if (s == NULL) {
    return false;  // not a synced list
  }
  checkRestoreTimeout(s);
  if (type == "status") {
    // Only the answer to the open query may cause traffic: a status for an
    // earlier query or an unsolicited one is ignored
    if (s->state != QUARK_SYNC_QUERIED || !msg["mid"].is<unsigned long>() ||
        msg["mid"].as<unsigned long>() != s->queryMid) {
      return false;
    }
    s->queryMid = 0;  // answered; a duplicate is ignored
    return handleStatus(s, msg);
  }
  if (type == "ack") {
    return handleAck(s, msg);
  }
  if (type == "request") {
    return handleRequest(s, msg);
  }
  if (type == "snapshot") {
    return handleSnapshot(s, msg);
  }
  return false;
}

// Authoritative cloud state, answer to a query (state QUERIED)
bool DBSyncManager::handleStatus(SyncListState* s, JsonObject msg) {
  String cloudInst = msg["inst"] | "";
  long cloudSeq = msg["seq"] | 0L;
  if (s->inst.length() == 0) {
    // Tombstone: drop the deleted instance if the cloud still holds it.
    // Anything else there is not ours to touch; the tombstone is done.
    if (DBOplog::inHistory(s, cloudInst)) {
      return sendDrop(s, cloudInst);
    }
    return oplog->disable(s->name);
  }
  if (cloudInst.length() == 0) {
    // Cloud has no copy: device is master
    return pushSnapshot(s) >= 0;
  }
  if (cloudInst != s->inst) {
    if (DBOplog::inHistory(s, cloudInst)) {
      // An instance this device deleted on purpose: replace it, never restore it
      Serial.println("Sync: cloud holds a deleted instance, sending snapshot ->" + s->name);
      return pushSnapshot(s) >= 0;
    }
    // Unknown to this device, e.g. the flash was wiped
    if (restoreAllowed(s)) {
      Serial.println("Sync: unknown cloud instance, restoring ->" + s->name);
      return sendRestore(s);
    }
    Serial.println("Sync: unknown cloud instance, sending snapshot ->" + s->name);
    return pushSnapshot(s) >= 0;
  }
  if (s->lost) {
    // Same instance, but the local file was lost and created again: the
    // cloud copy is the complete one
    if (restoreAllowed(s)) {
      Serial.println("Sync: local list was lost, restoring ->" + s->name);
      return sendRestore(s);
    }
    return pushSnapshot(s) >= 0;
  }
  if (cloudSeq > s->seq) {
    // Cloud is ahead of a master that never went back: realign it
    return pushSnapshot(s) >= 0;
  }
  s->cloudSeq = cloudSeq;
  s->state = QUARK_SYNC_IN_SYNC;
  settled(s);
  if (cloudSeq == s->seq) {
    return true;
  }
  return pushPending(s) >= 0;
}

// The cloud found a hole and asks for sequence numbers "from".."to". The
// device can only replay what its oplog still holds from "from" onwards,
// so this is the status case "cloud has from - 1": pushPending() replays
// the range (and everything after it) or falls back to a snapshot when
// the oldest entry left is newer than "from". Throttled per list.
bool DBSyncManager::handleRequest(SyncListState* s, JsonObject msg) {
  String cloudInst = msg["inst"] | "";
  if (s->inst.length() == 0 || s->lost || cloudInst != s->inst || !msg["from"].is<long>()) {
    return false;
  }
  if (s->state == QUARK_SYNC_RESTORING) {
    return false;
  }
  if (s->lastRequestAt != 0 && millis() - s->lastRequestAt < config.minCloudRequestIntervalMs) {
    Serial.println("Sync: cloud request throttled ->" + s->name);
    return false;
  }
  s->lastRequestAt = millis();
  long from = msg["from"].as<long>();
  if (from < 1) {
    from = 1;
  }
  if (from > s->seq + 1) {
    // Asked for numbers this instance never handed out: realign the cloud
    return pushSnapshot(s) >= 0;
  }
  s->cloudSeq = from - 1;
  s->state = QUARK_SYNC_IN_SYNC;
  return pushPending(s) >= 0;
}

// Acknowledgement of one published message, identified by its "mid". The
// seq that message carried becomes the known cloud position if it is
// further than what is known; older or repeated acks change nothing, so
// the order in which acks arrive does not matter. Never causes a message.
bool DBSyncManager::handleAck(SyncListState* s, JsonObject msg) {
  if (s->inst.length() == 0 || s->lost || !msg["mid"].is<unsigned long>()) {
    return false;
  }
  if (msg["inst"].is<const char*>() && String(msg["inst"] | "") != s->inst) {
    return false;
  }
  long seq = DBOplog::seqOfSent(s, msg["mid"].as<unsigned long>());
  if (seq < 0) {
    return false;  // not a message of this instance, or too old to matter
  }
  if (seq > s->cloudSeq) {
    s->cloudSeq = seq;
  }
  return true;
}

void DBSyncManager::abortRestore(SyncListState* s) {
  String restoreFile = dbUtils->getRestoreFileName(s->name);
  if (SPIFFS.exists(restoreFile)) {
    SPIFFS.remove(restoreFile);
  }
  s->state = QUARK_SYNC_UNKNOWN;
  s->restoreMid = 0;
  s->restoreChunk = 0;
  s->restoreBytes = 0;
  s->restoreCrc = 0;
  s->restoreCount = 0;
}

// Snapshot chunks from the cloud, only while a restore was requested.
// Records are staged in "<list>.rst"; the last chunk installs the file
// through the same ".new" swap that update/delete use, then the list
// adopts the cloud instance id and sequence number.
bool DBSyncManager::handleSnapshot(SyncListState* s, JsonObject msg) {
  if (s->state != QUARK_SYNC_RESTORING) {
    return false;
  }
  // Chunks answer one specific restore request
  if (!msg["mid"].is<unsigned long>() || msg["mid"].as<unsigned long>() != s->restoreMid) {
    return false;
  }
  long index = msg["i"] | 0L;
  bool last = msg["last"] | false;
  if (index != s->restoreChunk) {
    Serial.println("ERROR: Sync snapshot chunk out of order, restore aborted ->" + s->name);
    abortRestore(s);
    return false;
  }
  if (index == 0) {
    s->restoreInst = String(msg["inst"] | "");
    if (s->restoreInst.length() == 0) {
      Serial.println("ERROR: Sync snapshot without instance id, restore aborted ->" + s->name);
      abortRestore(s);
      return false;
    }
  }
  if (msg["seq"].is<long>()) {
    s->restoreSeq = msg["seq"].as<long>();
  }
  String restoreFile = dbUtils->getRestoreFileName(s->name);
  File file = SPIFFS.open(restoreFile, index == 0 ? "w" : "a");
  if (!file) {
    Serial.println("ERROR: Sync could not stage snapshot ->" + s->name);
    abortRestore(s);
    return false;
  }
  bool ok = true;
  JsonArray docs = msg["docs"].as<JsonArray>();
  for (JsonVariant doc : docs) {
    if (!doc.is<JsonObject>()) {
      continue;
    }
    String line;
    serializeJson(doc, line);
    s->restoreBytes += line.length() + 2;
    if (s->restoreBytes > config.maxRestoreBytes) {
      ok = false;
      break;
    }
    ok = file.println(line) == line.length() + 2;
    if (!ok) {
      break;
    }
    s->restoreCrc = DBUtils::crc32(s->restoreCrc, line.c_str(), line.length());
    s->restoreCrc = DBUtils::crc32(s->restoreCrc, "\n", 1);
    s->restoreCount++;
  }
  file.close();
  if (!ok) {
    Serial.println("ERROR: Sync snapshot too large or not writable, restore aborted ->" + s->name);
    abortRestore(s);
    return false;
  }
  if (!last) {
    s->restoreChunk++;
    return true;
  }
  // Integrity check before anything local is touched: the last chunk must
  // carry the CRC-32 and document count of the whole snapshot
  if (!msg["crc"].is<unsigned long>() || !msg["count"].is<long>()) {
    Serial.println("ERROR: Sync snapshot without checksum, restore discarded ->" + s->name);
    abortRestore(s);
    return false;
  }
  if (msg["crc"].as<unsigned long>() != s->restoreCrc || msg["count"].as<long>() != s->restoreCount) {
    Serial.println("ERROR: Sync snapshot checksum mismatch, restore discarded ->" + s->name);
    abortRestore(s);
    return false;
  }
  dbUtils->recoverList(s->name);
  if (!SPIFFS.rename(restoreFile, dbUtils->getNewFileName(s->name)) || !dbUtils->installNewFile(s->name)) {
    Serial.println("ERROR: Sync could not install snapshot ->" + s->name);
    abortRestore(s);
    return false;
  }
  oplog->clearLog(s->name);
  s->inst = s->restoreInst;
  s->seq = s->restoreSeq;
  s->history = "";
  s->lost = false;
  oplog->save(s);
  DBOplog::forgetSent(s);  // acks for the previous instance no longer apply
  s->cloudSeq = s->seq;
  s->state = QUARK_SYNC_IN_SYNC;
  s->restoreMid = 0;
  s->restoreChunk = 0;
  s->restoreBytes = 0;
  Serial.println("Sync: list restored from cloud ->" + s->name);
  return true;
}

// MQTT topic filter matching: "+" matches one level, "#" the rest
bool DBSyncManager::topicMatches(const char* filter, const char* topic) {
  if (filter == NULL || topic == NULL) {
    return false;
  }
  while (*filter != 0) {
    if (*filter == '#') {
      return true;
    }
    if (*filter == '+') {
      while (*topic != 0 && *topic != '/') {
        topic++;
      }
      filter++;
      continue;
    }
    if (*topic == 0) {
      // "a/#" also matches "a"
      return filter[0] == '/' && filter[1] == '#' && filter[2] == 0;
    }
    if (*filter != *topic) {
      return false;
    }
    filter++;
    topic++;
  }
  return *topic == 0;
}
