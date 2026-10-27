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

//This is the implementation for all write operations
#include "write_processor.h"
#include "../utils/db_utils.h"
#include "../filter/filter_processor.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif



DBWriteProcessor::DBWriteProcessor(byte fileMode) {
  this->fileMode = fileMode;
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
  dbUtils = new DBUtils();
  dbFilter = new DBFilterProcessor();
}
// Create a list in the filesystem
bool DBWriteProcessor::createList(String listName) {
  listName.trim();
  if(!dbUtils->validateListName(listName.c_str())) {
    return false;
  }
  dbUtils->recoverList(listName);
  File file;
  String fileName = dbUtils->getListFileName(listName);
  if(SPIFFS.exists(fileName)) {
    Serial.println("ERROR:List already exists ->" + listName );
    return false;
  }
  file = SPIFFS.open(fileName, "w");
  if (!file) {
    Serial.println("Error opening file for writing");
    return false;
  }
  file.close();
  return true;
}

// Remove a list in the filesystem
bool DBWriteProcessor::deleteList(String listName) {
  listName.trim();
  if(!dbUtils->validateListName(listName.c_str())) {
    return false;
  }
  dbUtils->recoverList(listName);
  String fileName = dbUtils->getListFileName(listName);
  if(!SPIFFS.exists(fileName)) {
    Serial.println("ERROR:List does not exist ->" + listName );
    return false;
  }
  // Remove any temp file first so it can never be recovered as the list
  String tempFileName = dbUtils->getTempFileName(listName);
  if(SPIFFS.exists(tempFileName)) {
    SPIFFS.remove(tempFileName);
  }
  return SPIFFS.remove(fileName);
}

// Update records in the list
int DBWriteProcessor::updateRecords(String listName , String selector,JsonObject* jsonObject) {
  JsonDocument filterDoc;
  DeserializationError error = deserializeJson(filterDoc, selector);
  if (error) {
    Serial.println("ERROR: Invalid selector or size more than 500 bytes->" + selector);
    return -1;
  }
  String updateObjectJson;
  serializeJson((*jsonObject), updateObjectJson);
  return this->updateFromJson(listName, &filterDoc , updateObjectJson);
}

// Update records in the list
int DBWriteProcessor::updateFromJson(String listName, JsonDocument* filterDoc, String updateObjectJson) {
  return this->rewriteList(listName, filterDoc, &updateObjectJson);
}

// Delete records in the list
int DBWriteProcessor::deleteRecords(String listName, String selector) {
 JsonDocument filterDoc;
  DeserializationError error = deserializeJson(filterDoc, selector);
  if (error) {
    Serial.println("ERROR: Invalid filter->" + selector);
    return -1;
  }
  return this->deleteFromJson(listName, &filterDoc);
}
// Delete records in the list
int DBWriteProcessor::deleteFromJson(String listName, JsonDocument* filterDoc) {
  return this->rewriteList(listName, filterDoc, NULL);
}

// Rewrite the list with matching records replaced by updateObjectJson, or
// removed when updateObjectJson is NULL. Returns the number of matches.
// The new contents go to a temp file that only replaces the list once fully
// written, so a power loss never leaves the list truncated.
int DBWriteProcessor::rewriteList(String listName, JsonDocument* filterDoc, const String* updateObjectJson) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return -1;
  }
  dbUtils->recoverList(listName);
  String fileName = dbUtils->getListFileName(listName);
  String tempFileName = dbUtils->getTempFileName(listName);
  if (!SPIFFS.exists(fileName)) {
    return -1;
  }
  File dataFile = SPIFFS.open(fileName, "r");
  if (!dataFile) {
    Serial.println("Error opening file for reading");
    return -1;
  }
  File tempFile = SPIFFS.open(tempFileName, "w");
  if (!tempFile) {
    Serial.println("Error opening file for writing");
    dataFile.close();
    return -1;
  }
  JsonObject root = filterDoc->as<JsonObject>();
  int matchCount = 0;
  bool writeOk = true;
  while (writeOk && dataFile.available()) {
    String jsonString = dataFile.readStringUntil('\n');
    jsonString.trim();
    JsonDocument jsonLineDocument;
    DeserializationError error = deserializeJson(jsonLineDocument, jsonString);
    if(error || jsonString == "") {
      continue;
    }
    bool mismatch = dbFilter->filterParse(root, jsonLineDocument, false);
    if (!mismatch) {
      matchCount++;
      if (updateObjectJson == NULL) {
        continue;
      }
      jsonString = *updateObjectJson;
      Serial.println(" updating object" + String(matchCount));
    }
    writeOk = tempFile.println(jsonString) == jsonString.length() + 2;
  }
  dataFile.close();
  tempFile.close();
  if (!writeOk) {
    Serial.println("Error writing file ->" + tempFileName);
    SPIFFS.remove(tempFileName);
    return -1;
  }
  // Nothing changed, so skip the flash write
  if (matchCount == 0) {
    SPIFFS.remove(tempFileName);
    return 0;
  }
  if (!this->replaceListFile(fileName, tempFileName)) {
    // The temp file is kept so recoverList() can finish the swap later
    Serial.println("Error replacing list ->" + listName);
    return -1;
  }
  return matchCount;
}

// Swap the fully written temp file in for the list file. SPIFFS rename
// fails when the target exists, so the list file is removed first.
bool DBWriteProcessor::replaceListFile(String fileName, String tempFileName) {
  if (SPIFFS.exists(fileName) && !SPIFFS.remove(fileName)) {
    return false;
  }
  return SPIFFS.rename(tempFileName, fileName);
}

// Add record in the list
bool DBWriteProcessor::addRecord(String listName, JsonObject* jsonObject) {
  listName.trim();
  if(!dbUtils->validateListName(listName.c_str())) {
    Serial.println("ERROR:List Name Invalid ->" + listName );
    return false;
  }
  dbUtils->recoverList(listName);
  String fileName = dbUtils->getListFileName(listName);
  if(!SPIFFS.exists(fileName)) {
    Serial.println("ERROR:List does not exist ->" + listName );
    return false;
  }
  String jsonString;
  serializeJson((*jsonObject), jsonString);
  // A power loss during an earlier append can leave a partial last line with
  // no newline. Terminate it so this record starts on its own line.
  bool needsNewline = false;
  File file = SPIFFS.open(fileName, "r");
  if (file) {
    size_t fileSize = file.size();
    if (fileSize > 0 && file.seek(fileSize - 1)) {
      needsNewline = file.read() != '\n';
    }
    file.close();
  }
  file = SPIFFS.open(fileName, "a");
  if (!file) {
    Serial.println("Error opening file for writing");
    return false;
  }
  bool writeOk = true;
  if (needsNewline) {
    writeOk = file.println() == 2;
  }
  writeOk = writeOk && file.println(jsonString) == jsonString.length() + 2;
  Serial.println("writing record " +jsonString );
  file.close();
  return writeOk;
}

// Sets the max records the list can hold
// TBD: value is stored but not enforced by write operations yet
void DBWriteProcessor::setMaxRecords(int maxRecords) {
  this->maxRecords = maxRecords;
}

// Sets the max size of a single record
// TBD: value is stored but not enforced by write operations yet
void DBWriteProcessor::setMaxRecordSize(int maxRecordSize) {
  this->maxRecordSize = maxRecordSize;
}
