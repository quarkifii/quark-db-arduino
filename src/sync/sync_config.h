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

// Configuration of the cloud backup (sync) feature: the global settings
// passed to QuarkDB::enableSync() and the per list options passed to
// QuarkDB::enableListSync(). The transport itself is QuarkTransport.h.
#ifndef QUARK_DB_SYNC_CONFIG_H
#define QUARK_DB_SYNC_CONFIG_H
#include <Arduino.h>

// Per list oplog cap on flash. Oldest entries are dropped when exceeded.
#define __QUARKDB_DEFAULT_MAX_OPLOG_BYTES__  4096
// Largest MQTT payload the DB builds. Snapshots are split into chunks of at
// most this size. Must fit the MQTT client buffer (PubSubClient::setBufferSize).
#define __QUARKDB_DEFAULT_MAX_PAYLOAD_BYTES__  1024
// Number of lists that can have sync enabled at the same time
#define __QUARKDB_MAX_SYNC_LISTS__  8
// The sequence counter in "<list>.sync" is refreshed every this many changes;
// in between the last oplog line is the source of truth (saves a flash write per change)
#define __QUARKDB_SYNC_META_SAVE_EVERY__  32
// Instance ids a list name remembers as deliberately deleted (oldest dropped first)
#define __QUARKDB_MAX_INSTANCE_HISTORY__  8
// Recently published (message id, seq) pairs kept per list to correlate acks
#define __QUARKDB_SYNC_ACK_WINDOW__  8

// Whether a list may be rewritten from the cloud copy when the cloud holds a
// different instance of the list (e.g. the device flash was wiped).
enum QuarkRestoreMode : int8_t {
  QUARK_RESTORE_INHERIT = -1,      // per list default: use QuarkSyncConfig::restoreOnMismatch
  QUARK_RESTORE_NEVER = 0,         // device is master: push a snapshot to the cloud
  QUARK_RESTORE_ON_MISMATCH = 1    // pull the cloud copy and replace the local list
};

// Global sync configuration passed to QuarkDB::enableSync()
struct QuarkSyncConfig {
  String publishTopic;     // device -> cloud (ops, snapshots, queries)
  String subscribeTopic;   // cloud -> device (status, acks, snapshots); may use MQTT wildcards
  String deviceId;         // optional, added to every message as "dev"
  size_t maxOplogBytes = __QUARKDB_DEFAULT_MAX_OPLOG_BYTES__;
  size_t maxPayloadBytes = __QUARKDB_DEFAULT_MAX_PAYLOAD_BYTES__;
  bool restoreOnMismatch = false;  // default for lists using QUARK_RESTORE_INHERIT
  // How long a list waits for the cloud "status" after a query before a
  // change triggers a new query (0 = wait until the next syncCloud())
  unsigned long queryTimeoutMs = 30000;
  // ---- limits against a misbehaving cloud ----
  // Largest inbound message accepted (longer payloads are dropped unparsed)
  size_t maxInboundBytes = __QUARKDB_DEFAULT_MAX_PAYLOAD_BYTES__;
  // A restore is aborted once more than this many bytes were staged
  size_t maxRestoreBytes = 65536;
  // A restore whose chunks stop arriving is abandoned after this long
  unsigned long restoreTimeoutMs = 60000;
  // Cloud initiated "request" messages for one list are served at most once per interval
  unsigned long minCloudRequestIntervalMs = 5000;
};

// Per list options passed to QuarkDB::enableListSync()
struct QuarkListSyncOptions {
  QuarkRestoreMode restore = QUARK_RESTORE_INHERIT;
};

#endif
