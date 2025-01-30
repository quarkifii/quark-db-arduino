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
  File file;
  String fileName = "/__quarkdb__/" +listName +".jsonl";
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
  String fileName = "/__quarkdb__/" +listName + +".jsonl";
  if(!SPIFFS.exists(fileName)) {
    Serial.println("ERROR:List does not exist ->" + listName );
    return false;
  }
  SPIFFS.remove(fileName);
  return true;
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
  String finalString = "";
  int updateCount = 0;
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
        if (finalString == "") {
          finalString = finalString + jsonString;
        } else {
          finalString = finalString + "\n" + jsonString;
        }
        continue;
      } else {
       
        if (finalString == "") {
          finalString = finalString + updateObjectJson;
        } else {
          finalString = finalString + "\n" + updateObjectJson;
        }
        updateCount++;
         Serial.println(" updating object" + String(updateCount));
      }
    }
    dataFile.close();
    dataFile = SPIFFS.open(fileName, "w");
    if (!dataFile) {
      Serial.println("Error opening file for writing");
      return -1;
    }
    dataFile.println(finalString);
    dataFile.close();
  } else {
    Serial.println("Error opening database.json");
  }
  return updateCount;
}

// Delete records in the list
int DBWriteProcessor::deleteRecords(String listName, String selector) {
 JsonDocument filterDoc;
  DeserializationError error = deserializeJson(filterDoc, selector);
  if (error) {
    Serial.println("ERROR: Invalid filter->" + selector);
    return false;
  }
  return this->deleteFromJson(listName, &filterDoc);
}
// Delete records in the list
int DBWriteProcessor::deleteFromJson(String listName, JsonDocument* filterDoc) {
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
  String finalString = "";
  int deleteCount = 0;
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
        if (finalString == "") {
          finalString = finalString + jsonString;
        } else {
          finalString = finalString + "\n" + jsonString;
        }
        continue;
      } else {
        deleteCount++;
      }
    }
    dataFile.close();
    dataFile = SPIFFS.open(fileName, "w");
    if (!dataFile) {
      Serial.println("Error opening file for writing");
      return -1;
    }
    dataFile.println(finalString);
    dataFile.close();
  } else {
    Serial.println("Error opening database.json");
  }
  return deleteCount;
}

// Add record in the list
bool DBWriteProcessor::addRecord(String listName, JsonObject* jsonObject) {
  listName.trim();
  if(!dbUtils->validateListName(listName.c_str())) {
    Serial.println("ERROR:List Name Invalid ->" + listName );
    return false;
  }
  String fileName = "/__quarkdb__/" +listName + +".jsonl";
  if(!SPIFFS.exists(fileName)) {
    Serial.println("ERROR:List does not exist ->" + listName );
    return false;
  }
  String jsonString;
  File file;
  serializeJson((*jsonObject), jsonString);
  file = SPIFFS.open(fileName, "a");
  if (!file) {
    Serial.println("Error opening file for writing");
    return false;
  }
  file.println(jsonString);
  Serial.println("writing record " +jsonString );
  file.close();
  return true;
}
