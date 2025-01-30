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
    return false;
  }
  File dataFile;
  String fileName = "/__quarkdb__/" +listName + +".jsonl";
  if (!SPIFFS.exists(fileName)) {
    return false;
  }
  dataFile = SPIFFS.open(fileName, "r");
  String finalString = "[";
  byte rowCount = 0;
  if (dataFile) {
    while (dataFile.available()) {
      String jsonString = dataFile.readStringUntil('\n');
      JsonDocument jsonLineDocument;
      DeserializationError error = deserializeJson(jsonLineDocument, jsonString);
      if(error || jsonString == "") {
        continue;
      }
      JsonObject root = filterDoc->as<JsonObject>();
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
  } else {
    Serial.println("Error opening database.json");
  }
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
  File dataFile;
  String fileName = "/__quarkdb__/" +listName + +".jsonl";
  if (!SPIFFS.exists(fileName)) {
    return -1;
  }
  dataFile = SPIFFS.open(fileName, "r");
  byte rowCount = 0;
  if (dataFile) {
    while (dataFile.available()) {
      String jsonString = dataFile.readStringUntil('\n');
      if(jsonString == "") {
        continue;
      }
      rowCount++;
    }
    dataFile.close();
  } else {
    Serial.println("Error opening database.json");
  }

  return rowCount;
}

String DBReadProcessor::getListNames() {
  String lists = "Lists found are..";
  #if defined (ESP8266)
  Dir dir = SPIFFS.openDir("/");
     while (dir.next()) {
       if (dir.fileName().startsWith("/__quarkdb__")) {
         lists = lists + "\n" + dir.fileName().substring(dir.fileName().lastIndexOf("/") + 1, dir.fileName().lastIndexOf("."));
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
    String fileName = String(file.path());
    while(file){
     if(fileName.startsWith("/__quarkdb__")) {
        lists = lists + "\n" + fileName.substring(fileName.lastIndexOf("/") + 1, fileName.lastIndexOf("."));
     }
     file = root.openNextFile();
    }
    if(lists == "Lists found are..") {
      return " No Lists Found";
    }
    return lists;
  #endif
}



