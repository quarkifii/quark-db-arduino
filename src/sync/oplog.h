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

// Per list operation log used by the cloud backup.
//
// A list with sync enabled has two extra files next to its ".jsonl":
//  - "<list>.sync"  metadata: {"inst":..,"seq":..,"restore":..,"hist":..,"lost":..}
//      inst     id of this incarnation of the list, generated when sync is
//               enabled; the cloud stores it with its copy. "" once the
//               list was deleted (tombstone) until the cloud has been told
//      seq      last sequence number handed out to a change
//      restore  QuarkRestoreMode for this list
//      hist     comma separated ids of earlier incarnations this device
//               deleted on purpose (newest last, at most
//               __QUARKDB_MAX_INSTANCE_HISTORY__). The cloud may still hold
//               one of them; such a copy is never restored. Cleared once the
//               cloud is known to hold the current incarnation.
//      lost     1 when the list file disappeared without deleteList() and
//               was created again: the cloud copy of the same incarnation
//               is the better one and may be restored
//  - "<list>.oplog" one JSON line per change, oldest first:
//      {"seq":5,"op":"add","doc":{...}}
//      {"seq":6,"op":"update","sel":{...},"doc":{...},"n":2}
//      {"seq":7,"op":"delete","sel":{...},"n":1}
//      {"seq":8,"op":"drop"}
//    The oplog is capped at maxBytes: when an append would exceed it, the
//    oldest lines are dropped (rollover). Missing sequence numbers are
//    detected by the sync layer, which then sends a full snapshot.
//
// The sequence counter in the metadata is only a lower bound: it is written
// when sync is enabled, every __QUARKDB_SYNC_META_SAVE_EVERY__ changes, when
// an entry is too large to be stored and on restore/drop. The newest oplog
// line always carries the real value, so the counter is reconciled from the
// oplog on load and never goes backwards, with one flash write per change
// instead of two.
#ifndef QUARK_DB_OPLOG_H
#define QUARK_DB_OPLOG_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif
#include "../utils/db_utils.h"
#include "sync_config.h"

enum QuarkSyncState : uint8_t {
  QUARK_SYNC_UNKNOWN = 0,   // cloud state not known; a query is needed
  QUARK_SYNC_QUERIED = 1,   // query sent, waiting for the cloud status
  QUARK_SYNC_IN_SYNC = 2,   // cloud state known; changes are pushed as they happen
  QUARK_SYNC_RESTORING = 3  // waiting for snapshot chunks from the cloud
};

struct SyncListState {
  // persisted in "<list>.sync"
  String name;
  String inst;       // "" for a tombstone (list deleted, cloud not yet told)
  String history;    // deliberately deleted ids, comma separated, newest last
  long seq;
  int8_t restore;
  bool lost;
  // runtime only
  uint8_t state;
  long cloudSeq;     // last seq the cloud is known to have, -1 if unknown
  unsigned long queriedAt;       // millis() of the last query, to time out a missing answer
  unsigned long queryMid;        // message id of the query whose answer is awaited
  unsigned long restoreMid;      // message id of the restore request the chunks must carry
  unsigned long ackMid[__QUARKDB_SYNC_ACK_WINDOW__];  // recently published messages ...
  long ackSeq[__QUARKDB_SYNC_ACK_WINDOW__];           // ... and the seq each one carried
  uint8_t ackPos;
  unsigned long lastRequestAt;   // millis() of the last cloud "request" served
  unsigned long restoreStartedAt;
  size_t restoreBytes;           // staged so far
  unsigned long restoreCrc;      // CRC-32 of what was staged so far
  long restoreCount;             // documents staged so far
  long restoreChunk; // next expected chunk index while restoring
  String restoreInst;
  long restoreSeq;
  bool used;
};

class DBOplog {
  private:
    DBUtils* dbUtils;
    size_t maxBytes;
    SyncListState lists[__QUARKDB_MAX_SYNC_LISTS__];
    SyncListState* find(const String& listName);
    SyncListState* allocate(const String& listName);
    bool load(SyncListState* state);
    bool rollover(const String& listName, size_t keepBytes);
    long lastSeqInLog(const String& listName);
  public:
    DBOplog();
    ~DBOplog();
    // Forget all cached state (what a reboot does)
    void reset();
    void setMaxBytes(size_t maxBytes);
    size_t getMaxBytes();
    // Cached state of a list, loaded from flash on first use. NULL if the
    // list has no sync metadata.
    SyncListState* get(const String& listName);
    // True if changes to the list are logged (metadata exists and is not a tombstone)
    bool isEnabled(const String& listName);
    // Create the metadata (new instance id) or update the options
    bool enable(const String& listName, QuarkRestoreMode restore);
    // Remove metadata and oplog
    bool disable(const String& listName);
    // Turn the metadata into a tombstone after the list was deleted; the
    // instance id joins the history
    bool markDropped(const String& listName);
    // The list file was created again while an instance was live
    bool markLost(const String& listName);
    // True if the id is one this device deleted on purpose
    static bool inHistory(const SyncListState* state, const String& inst);
    // Forget the history (and the lost flag) once the cloud holds the current instance
    bool clearHistory(SyncListState* state);
    // Ack correlation window: remember a published (mid, seq), look one up (-1 if unknown)
    static void rememberSent(SyncListState* state, unsigned long mid, long seq);
    static long seqOfSent(const SyncListState* state, unsigned long mid);
    static void forgetSent(SyncListState* state);
    // Persist the metadata fields of a state
    bool save(SyncListState* state);
    // Append a change. Returns its sequence number, or -1 if the list is not synced
    long append(const String& listName, const char* op, const String* selectorJson, const String* docJson, int count);
    bool clearLog(const String& listName);
    // Open the oplog for reading, oldest entry first
    File openLog(const String& listName);
    // Sequence number of an oplog line, or -1 if the line is not an entry
    static long parseSeq(const String& line);
    // Number of entries with a sequence number above fromSeq
    int countAfter(const String& listName, long fromSeq);
    // Load every "<list>.sync" found on flash into the cache
    int loadAll();
    int size();
    SyncListState* at(int index);
};

#endif
