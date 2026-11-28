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

#ifndef QUARK_DB_H
#define QUARK_DB_H

#include "reader/read_processor.h"
#include "writer/write_processor.h"
#include "command/command_parser.h"
#include "QuarkTransport.h"
#include "sync/sync_config.h"
#include "sync/oplog.h"
#include "sync/sync_manager.h"
#include <Arduino.h>
#include <ArduinoJson.h>


class QuarkDB {
  
  private:
    byte fileMode;
    int maxRecords;
    int maxRecordSize;
    String databaseName;
    DBReadProcessor* readProcessor;
    DBWriteProcessor* writeProcessor;
    CommandParser* commandParser;
    DBOplog oplog;
    DBSyncManager syncManager;
  public:
    QuarkDB();
    bool init(byte mode);
    bool createList(String listName);
    void processSerialCommand();
    bool deleteList(String listName);
    int getRecordCount(String listName);
    int getRecords(String listName, String selector, JsonDocument* resultDocument);
    int getRecords(String listName, String selector, JsonDocument* resultDocument , int limitRows);
    int updateRecords(String listName , String selector,JsonObject document);
    int deleteRecords(String listName , String selector);
    bool addRecord(String listName, JsonObject document);
    bool removeRecords(String listName); 
    bool removeAllLists(); 
    void setMaxRecords(int maxRecords);
    void setMaxRecordSize(int maxRecordSize);       
    JsonDocument * getDBStat();

    // ---- Cloud backup over MQTT (see sync/sync_manager.h for the protocol) ----
    // Attach the transport and configuration. Lists still need enableListSync().
    bool enableSync(QuarkTransport* transport, const QuarkSyncConfig& config);
    // Detach the transport; changes keep accumulating in the oplogs
    void disableSync();
    bool isSyncEnabled();
    // Start logging changes of an existing list. Persists across reboots.
    bool enableListSync(String listName, QuarkListSyncOptions options = QuarkListSyncOptions());
    // Stop logging and forget the oplog of a list (the cloud copy is left as is)
    bool disableListSync(String listName);
    bool isListSyncEnabled(String listName);
    // Call once the MQTT connection is (re)established: subscribes and asks
    // the cloud what it holds for every synced list. Returns the number of
    // lists processed or -1 when offline / sync not enabled.
    int syncCloud();
    // Check whether a received payload is a QuarkDB sync message
    // (JSON object with "__quarkdb__msg__": true).
    bool isQuarkDBMessage(const char* topic, String payload);
    bool isQuarkDBMessage(const char* topic, const uint8_t* payload, unsigned int length);
    // Feed every message received on the subscribe topic. Returns true if
    // the message was a sync message and was handled.
    bool handleMessage(const char* topic, String payload);
    bool handleMessage(const char* topic, const uint8_t* payload, unsigned int length);
    // On demand pull of the cloud copy, which replaces the local list once
    // its snapshot arrives through handleMessage(). 1 = requested, 0 = offline,
    // -1 = list not synced.
    int restoreFromCloud(String listName);
    // Sync state of a list: inst, seq, cloudSeq, state, pending, restore, dropped
    bool getSyncInfo(String listName, JsonDocument* info);
};

#endif
