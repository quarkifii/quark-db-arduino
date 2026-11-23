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

// Oplog implementation, see oplog.h for the file formats
#include "oplog.h"
#include <ArduinoJson.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif

// Whole file as a String. Stream::readString() is not used because on ESP32
// it waits for the stream timeout at end of file.
static String readWholeFile(File& file) {
  String content;
  content.reserve(file.size());
  while (file.available()) {
    content += (char)file.read();
  }
  return content;
}

DBOplog::DBOplog() {
  dbUtils = new DBUtils();
  maxBytes = __QUARKDB_DEFAULT_MAX_OPLOG_BYTES__;
  reset();
}

DBOplog::~DBOplog() {
  delete dbUtils;
}

void DBOplog::reset() {
  for (int i = 0; i < __QUARKDB_MAX_SYNC_LISTS__; i++) {
    lists[i].used = false;
  }
}

void DBOplog::setMaxBytes(size_t maxBytes) {
  this->maxBytes = maxBytes;
}

size_t DBOplog::getMaxBytes() {
  return maxBytes;
}

int DBOplog::size() {
  int n = 0;
  for (int i = 0; i < __QUARKDB_MAX_SYNC_LISTS__; i++) {
    if (lists[i].used) {
      n++;
    }
  }
  return n;
}

// index counts used slots only
SyncListState* DBOplog::at(int index) {
  int n = 0;
  for (int i = 0; i < __QUARKDB_MAX_SYNC_LISTS__; i++) {
    if (lists[i].used && n++ == index) {
      return &lists[i];
    }
  }
  return NULL;
}

SyncListState* DBOplog::find(const String& listName) {
  for (int i = 0; i < __QUARKDB_MAX_SYNC_LISTS__; i++) {
    if (lists[i].used && lists[i].name == listName) {
      return &lists[i];
    }
  }
  return NULL;
}

SyncListState* DBOplog::allocate(const String& listName) {
  for (int i = 0; i < __QUARKDB_MAX_SYNC_LISTS__; i++) {
    if (!lists[i].used) {
      SyncListState* s = &lists[i];
      s->used = true;
      s->name = listName;
      s->inst = "";
      s->history = "";
      s->seq = 0;
      s->restore = QUARK_RESTORE_INHERIT;
      s->lost = false;
      s->state = QUARK_SYNC_UNKNOWN;
      s->cloudSeq = -1;
      s->queriedAt = 0;
      s->queryMid = 0;
      s->restoreMid = 0;
      forgetSent(s);
      s->lastRequestAt = 0;
      s->restoreStartedAt = 0;
      s->restoreBytes = 0;
      s->restoreCrc = 0;
      s->restoreCount = 0;
      s->restoreChunk = 0;
      s->restoreInst = "";
      s->restoreSeq = 0;
      return s;
    }
  }
  Serial.println("ERROR: Too many synced lists, max " + String(__QUARKDB_MAX_SYNC_LISTS__));
  return NULL;
}

// Sequence number of the newest oplog entry, -1 if there is none
long DBOplog::lastSeqInLog(const String& listName) {
  long last = -1;
  File file = openLog(listName);
  if (!file) {
    return last;
  }
  while (file.available()) {
    String line = file.readStringUntil('\n');
    long seq = parseSeq(line);
    if (seq > last) {
      last = seq;
    }
  }
  file.close();
  return last;
}

// Reads the metadata file into the state and reconciles the sequence
// counter with the oplog (the oplog is appended first, so it may be ahead
// after a power loss)
bool DBOplog::load(SyncListState* state) {
  String fileName = dbUtils->getSyncMetaFileName(state->name);
  File file = SPIFFS.open(fileName, "r");
  if (!file) {
    return false;
  }
  String content = readWholeFile(file);
  file.close();
  JsonDocument doc;
  if (deserializeJson(doc, content)) {
    Serial.println("ERROR: Invalid sync metadata ->" + fileName);
    return false;
  }
  state->inst = String(doc["inst"] | "");
  state->history = String(doc["hist"] | "");
  state->seq = doc["seq"] | 0L;
  state->restore = (int8_t)(doc["restore"] | (int)QUARK_RESTORE_INHERIT);
  state->lost = (doc["lost"] | 0) != 0;
  long last = lastSeqInLog(state->name);
  if (last > state->seq) {
    state->seq = last;
    save(state);
  }
  // A staging file from an interrupted restore is useless without its state
  String restoreFile = dbUtils->getRestoreFileName(state->name);
  if (SPIFFS.exists(restoreFile)) {
    SPIFFS.remove(restoreFile);
  }
  return true;
}

bool DBOplog::save(SyncListState* state) {
  String json = "{\"inst\":\"" + state->inst + "\",\"seq\":" + String(state->seq) +
                ",\"restore\":" + String((int)state->restore);
  if (state->history.length() > 0) {
    json += ",\"hist\":\"" + state->history + "\"";
  }
  if (state->lost) {
    json += ",\"lost\":1";
  }
  json += "}";
  File file = SPIFFS.open(dbUtils->getSyncMetaFileName(state->name), "w");
  if (!file) {
    Serial.println("ERROR: Could not write sync metadata ->" + state->name);
    return false;
  }
  bool ok = file.print(json) == json.length();
  file.close();
  return ok;
}

SyncListState* DBOplog::get(const String& listName) {
  SyncListState* s = find(listName);
  if (s != NULL) {
    return s;
  }
  if (!SPIFFS.exists(dbUtils->getSyncMetaFileName(listName))) {
    return NULL;
  }
  s = allocate(listName);
  if (s == NULL) {
    return NULL;
  }
  if (!load(s)) {
    s->used = false;
    return NULL;
  }
  return s;
}

bool DBOplog::isEnabled(const String& listName) {
  SyncListState* s = get(listName);
  return s != NULL && s->inst.length() > 0;
}

bool DBOplog::enable(const String& listName, QuarkRestoreMode restore) {
  SyncListState* s = get(listName);
  if (s == NULL) {
    s = allocate(listName);
    if (s == NULL) {
      return false;
    }
  }
  if (s->inst.length() == 0) {
    // New instance: fresh sequence numbers. A tombstone keeps its history
    // so the cloud copy of a deleted instance is never restored.
    s->inst = dbUtils->generateInstanceId();
    s->seq = 0;
    s->lost = false;
    s->state = QUARK_SYNC_UNKNOWN;
    s->cloudSeq = -1;
    forgetSent(s);
    clearLog(listName);
  }
  s->restore = (int8_t)restore;
  if (!save(s)) {
    s->used = false;
    return false;
  }
  return true;
}

bool DBOplog::disable(const String& listName) {
  SyncListState* s = get(listName);
  if (s == NULL) {
    return false;
  }
  s->used = false;
  String metaFile = dbUtils->getSyncMetaFileName(listName);
  bool ok = SPIFFS.remove(metaFile);
  String logFile = dbUtils->getOplogFileName(listName);
  if (SPIFFS.exists(logFile)) {
    SPIFFS.remove(logFile);
  }
  String restoreFile = dbUtils->getRestoreFileName(listName);
  if (SPIFFS.exists(restoreFile)) {
    SPIFFS.remove(restoreFile);
  }
  return ok;
}

// The list is gone: remember the instance as deliberately deleted so the
// cloud can be told and its copy is never restored, and stop logging
bool DBOplog::markDropped(const String& listName) {
  SyncListState* s = get(listName);
  if (s == NULL || s->inst.length() == 0) {
    return false;
  }
  // Bounded history, oldest dropped first
  int entries = 1;
  for (unsigned int i = 0; i < s->history.length(); i++) {
    if (s->history[i] == ',') {
      entries++;
    }
  }
  if (s->history.length() == 0) {
    entries = 0;
  }
  while (entries >= __QUARKDB_MAX_INSTANCE_HISTORY__) {
    int comma = s->history.indexOf(',');
    s->history = comma < 0 ? String("") : s->history.substring(comma + 1);
    entries--;
  }
  if (s->history.length() > 0) {
    s->history += ",";
  }
  s->history += s->inst;
  s->inst = "";
  s->lost = false;
  s->seq = s->seq + 1;
  s->state = QUARK_SYNC_UNKNOWN;
  s->cloudSeq = -1;
  forgetSent(s);
  clearLog(listName);
  return save(s);
}

// The list file was created again although an instance is live: it was
// lost (corruption, manual removal) rather than deleted through the API
bool DBOplog::markLost(const String& listName) {
  SyncListState* s = get(listName);
  if (s == NULL || s->inst.length() == 0) {
    return false;
  }
  s->lost = true;
  s->state = QUARK_SYNC_UNKNOWN;
  s->cloudSeq = -1;
  return save(s);
}

bool DBOplog::inHistory(const SyncListState* s, const String& inst) {
  if (inst.length() == 0 || s->history.length() == 0) {
    return false;
  }
  int from = 0;
  while (from <= (int)s->history.length()) {
    int comma = s->history.indexOf(',', from);
    String entry = comma < 0 ? s->history.substring(from) : s->history.substring(from, comma);
    if (entry == inst) {
      return true;
    }
    if (comma < 0) {
      break;
    }
    from = comma + 1;
  }
  return false;
}

void DBOplog::rememberSent(SyncListState* s, unsigned long mid, long seq) {
  s->ackMid[s->ackPos] = mid;
  s->ackSeq[s->ackPos] = seq;
  s->ackPos = (uint8_t)((s->ackPos + 1) % __QUARKDB_SYNC_ACK_WINDOW__);
}

long DBOplog::seqOfSent(const SyncListState* s, unsigned long mid) {
  if (mid == 0) {
    return -1;
  }
  for (int i = 0; i < __QUARKDB_SYNC_ACK_WINDOW__; i++) {
    if (s->ackMid[i] == mid) {
      return s->ackSeq[i];
    }
  }
  return -1;
}

void DBOplog::forgetSent(SyncListState* s) {
  for (int i = 0; i < __QUARKDB_SYNC_ACK_WINDOW__; i++) {
    s->ackMid[i] = 0;
    s->ackSeq[i] = -1;
  }
  s->ackPos = 0;
}

bool DBOplog::clearHistory(SyncListState* s) {
  if (s->history.length() == 0 && !s->lost) {
    return true;
  }
  s->history = "";
  s->lost = false;
  return save(s);
}

bool DBOplog::clearLog(const String& listName) {
  String logFile = dbUtils->getOplogFileName(listName);
  if (SPIFFS.exists(logFile)) {
    return SPIFFS.remove(logFile);
  }
  return true;
}

File DBOplog::openLog(const String& listName) {
  return SPIFFS.open(dbUtils->getOplogFileName(listName), "r");
}

// Entries always start with {"seq":N
long DBOplog::parseSeq(const String& line) {
  const char* prefix = "{\"seq\":";
  if (!line.startsWith(prefix)) {
    return -1;
  }
  const char* digits = line.c_str() + strlen(prefix);
  char* end = NULL;
  long seq = strtol(digits, &end, 10);
  if (end == digits || seq < 0) {
    return -1;
  }
  return seq;
}

int DBOplog::countAfter(const String& listName, long fromSeq) {
  int n = 0;
  File file = openLog(listName);
  if (!file) {
    return 0;
  }
  while (file.available()) {
    if (parseSeq(file.readStringUntil('\n')) > fromSeq) {
      n++;
    }
  }
  file.close();
  return n;
}

// Keeps only the newest whole lines that fit in keepBytes. The oplog is at
// most maxBytes so it is rewritten through memory.
bool DBOplog::rollover(const String& listName, size_t keepBytes) {
  String logFile = dbUtils->getOplogFileName(listName);
  File file = SPIFFS.open(logFile, "r");
  if (!file) {
    return false;
  }
  String all = readWholeFile(file);
  file.close();
  unsigned int cut = 0;
  if (all.length() > keepBytes) {
    cut = all.length() - keepBytes;
  }
  // Move to the start of the next line so no partial entry is kept
  if (cut > 0) {
    int newline = all.indexOf('\n', cut - 1);
    cut = newline < 0 ? all.length() : (unsigned int)newline + 1;
  }
  String kept = all.substring(cut);
  file = SPIFFS.open(logFile, "w");
  if (!file) {
    return false;
  }
  bool ok = file.print(kept) == kept.length();
  file.close();
  return ok;
}

long DBOplog::append(const String& listName, const char* op, const String* selectorJson, const String* docJson, int count) {
  SyncListState* s = get(listName);
  if (s == NULL || s->inst.length() == 0) {
    return -1;
  }
  long seq = s->seq + 1;
  String line = "{\"seq\":" + String(seq) + ",\"op\":\"" + op + "\"";
  if (selectorJson != NULL) {
    line += ",\"sel\":" + *selectorJson;
  }
  if (docJson != NULL) {
    line += ",\"doc\":" + *docJson;
  }
  if (count >= 0) {
    line += ",\"n\":" + String(count);
  }
  line += "}";
  size_t lineBytes = line.length() + 2;
  bool stored = false;
  if (lineBytes > maxBytes) {
    // Cannot be logged; the sequence gap makes the next sync send a snapshot
    Serial.println("WARN: Oplog entry larger than max oplog bytes, skipped ->" + listName);
  } else {
    String logFile = dbUtils->getOplogFileName(listName);
    size_t current = 0;
    File file = SPIFFS.open(logFile, "r");
    if (file) {
      current = file.size();
      file.close();
    }
    if (current + lineBytes > maxBytes && !rollover(listName, maxBytes - lineBytes)) {
      Serial.println("ERROR: Oplog rollover failed ->" + listName);
    }
    file = SPIFFS.open(logFile, "a");
    if (!file) {
      Serial.println("ERROR: Could not open oplog ->" + listName);
    } else {
      stored = file.println(line) == lineBytes;
      file.close();
    }
  }
  s->seq = seq;
  // The oplog line carries the number; the metadata is refreshed now and
  // then, and always when there is no line to recover the number from
  if (!stored || seq % __QUARKDB_SYNC_META_SAVE_EVERY__ == 0) {
    save(s);
  }
  return seq;
}

// Every "<list>.sync" file on flash becomes a cached state
int DBOplog::loadAll() {
  String prefix = "/__quarkdb__/";
  String suffix = ".sync";
  #if defined (ESP8266)
    Dir dir = SPIFFS.openDir("/");
    while (dir.next()) {
      String path = dir.fileName();
      if (path.startsWith(prefix) && path.endsWith(suffix)) {
        get(path.substring(prefix.length(), path.length() - suffix.length()));
      }
    }
  #elif defined(ESP32)
    File root = SPIFFS.open("/");
    if (root && root.isDirectory()) {
      File file = root.openNextFile();
      while (file) {
        String path = String(file.path());
        file.close();
        if (path.startsWith(prefix) && path.endsWith(suffix)) {
          get(path.substring(prefix.length(), path.length() - suffix.length()));
        }
        file = root.openNextFile();
      }
      root.close();
    }
  #endif
  return size();
}
