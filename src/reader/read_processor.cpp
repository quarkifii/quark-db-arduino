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

//Reader implementation
#include "read_processor.h"
#include "../utils/db_utils.h"
#include "../filter/filter_processor.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif

DBReadProcessor::DBReadProcessor(byte fileMode) {
  this->fileMode = fileMode;
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
  dbUtils = new DBUtils();
  dbFilter = new DBFilterProcessor();
}

//Get record count
int DBReadProcessor::getRecordCount(String listName) {
  return this->queryJsonCount(listName);
}

//Get records in json Array
int DBReadProcessor::getRecords(String listName, String filter, JsonDocument* resultDoc, int limitRows) {
  JsonDocument filterDoc;
  DeserializationError error = deserializeJson(filterDoc, filter);
  if (error) {
    Serial.println("ERROR: Invalid filter->" + filter);
    return -1;
  }
  return this->queryJson(listName, &filterDoc, resultDoc , limitRows);
}


//Get records in json Array
int DBReadProcessor::queryJson(String listName, JsonDocument* filterDoc, JsonDocument* resultDocument, int limitRows) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return -1;
  }
  dbUtils->recoverList(listName);
  String fileName = dbUtils->getListFileName(listName);
  if (!SPIFFS.exists(fileName)) {
    return -1;
  }
  File dataFile = SPIFFS.open(fileName, "r");
  if (!dataFile) {
    Serial.println("Error opening file for reading");
    return -1;
  }
  JsonObject root = filterDoc->as<JsonObject>();
  String finalString = "[";
  int rowCount = 0;
  while (dataFile.available()) {
    String jsonString = dataFile.readStringUntil('\n');
    jsonString.trim();
    JsonDocument jsonLineDocument;
    DeserializationError error = deserializeJson(jsonLineDocument, jsonString);
    if(error || jsonString == "") {
      continue;
    }
    bool mismatch = dbFilter->filterParse(root, jsonLineDocument, false);
    if (mismatch) {
      continue;
    } else if(++rowCount <= limitRows) {
      if (finalString == "[") {
        finalString = finalString + jsonString;
      } else {
        finalString = finalString + "\n," + jsonString;
      }
    }
  }
  dataFile.close();
  finalString = finalString + "]";
  DeserializationError err = deserializeJson(*resultDocument, finalString);
  if (err) {
    Serial.println("Not enough size or incorrect json document");
    return -1;
  }

  return rowCount;
}

//Query count
int DBReadProcessor::queryJsonCount(String listName) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return -1;
  }
  dbUtils->recoverList(listName);
  String fileName = dbUtils->getListFileName(listName);
  if (!SPIFFS.exists(fileName)) {
    return -1;
  }
  File dataFile = SPIFFS.open(fileName, "r");
  if (!dataFile) {
    Serial.println("Error opening file for reading");
    return -1;
  }
  int rowCount = 0;
  while (dataFile.available()) {
    String jsonString = dataFile.readStringUntil('\n');
    jsonString.trim();
    if(jsonString == "") {
      continue;
    }
    // Count only complete records so the count matches getRecords()
    JsonDocument jsonLineDocument;
    if (deserializeJson(jsonLineDocument, jsonString)) {
      continue;
    }
    rowCount++;
  }
  dataFile.close();

  return rowCount;
}

// List name for a file in the DB folder, or "" if the file is not a list.
// A ".new" file without its list file is a list whose rewrite was
// interrupted mid-swap; it is still listed and gets installed on its next
// access. ".tmp" files are incomplete and never listed.
String DBReadProcessor::listNameFromPath(String path) {
  if (!path.startsWith("/__quarkdb__/")) {
    return "";
  }
  String name = path.substring(path.lastIndexOf("/") + 1, path.lastIndexOf("."));
  if (path.endsWith(".jsonl")) {
    return name;
  }
  if (path.endsWith(".new") && !SPIFFS.exists(dbUtils->getListFileName(name))) {
    return name;
  }
  return "";
}

String DBReadProcessor::getListNames() {
  String lists = "Lists found are..";
  #if defined (ESP8266)
  Dir dir = SPIFFS.openDir("/");
     while (dir.next()) {
       String listName = listNameFromPath(dir.fileName());
       if (listName != "") {
         lists = lists + "\n" + listName;
       }
     }
     if(lists == "Lists found are..") {
       return " No Lists Found";
     }
     return lists;
  #elif defined(ESP32)
    File root = SPIFFS.open("/");
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return "ERROR : File error";
    }
    File file = root.openNextFile();
    String fileName;
    while(file){
     fileName = String(file.path());
     String listName = listNameFromPath(fileName);
     if(listName != "") {
        lists = lists + "\n" + listName;
     }
     file.close();
     file = root.openNextFile();
    }
    root.close();
    if(lists == "Lists found are..") {
      return " No Lists Found";
    }
    return lists;
  #endif
}





// Sets the max records the list can hold
// TBD: value is stored but not enforced by read operations yet
void DBReadProcessor::setMaxRecords(int maxRecords) {
  this->maxRecords = maxRecords;
}

// Sets the max size of a single record
// TBD: value is stored but not enforced by read operations yet
void DBReadProcessor::setMaxRecordSize(int maxRecordSize) {
  this->maxRecordSize = maxRecordSize;
}
