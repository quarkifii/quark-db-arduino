#include "read_processor.h"
#include "db_utils.h"
#include "filter_processor.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>

DBReadProcessor::DBReadProcessor(byte fileMode) {
  this->fileMode = fileMode;
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
  dbUtils = new DBUtils();
  dbFilter = new DBFilterProcessor();
}
void DBReadProcessor::init() {
}

int DBReadProcessor::getRecordCount(String listName) {
  return this->queryJsonCount(listName);
}

int DBReadProcessor::getRecords(String listName, String filter, DynamicJsonDocument* resultDoc, int limitRows) {
  DynamicJsonDocument filterDoc(__QUARKDB_FILTER_SIZE__);
  DeserializationError error = deserializeJson(filterDoc, filter);
  if (error) {
    Serial.println("ERROR: Invalid filter->" + filter);
    return -1;
  }
  return this->queryJson(listName, &filterDoc, resultDoc , limitRows);
}



int DBReadProcessor::queryJson(String listName, DynamicJsonDocument* filterDoc, DynamicJsonDocument* resultDocument, int limitRows) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return false;
  }
  File dataFile;
  String fileName = "/__quarkdb__/" +listName + +".jsonl";
  if (!SPIFFS.exists(fileName)) {
    // Serial.println("ERROR:List does not exist ->" + listName);
    return false;
  }

  if (this->fileMode == 2) {
    dataFile = SPIFFS.open(fileName, "r");
  }
  String finalString = "[";
  byte rowCount = 0;
  if (dataFile) {
    while (dataFile.available()) {
      String jsonString = dataFile.readStringUntil('\n');
      //Serial.println(" read line is " + jsonString);
      DynamicJsonDocument jsonLineDocument(this->maxRecordSize);
      DeserializationError error = deserializeJson(jsonLineDocument, jsonString);
      if(error || jsonString == "") {
        continue;
      }
      JsonObject root = filterDoc->as<JsonObject>();
      bool mismatch = dbFilter->filterParse(root, jsonLineDocument, false);
      if (mismatch) {
        continue;
      } else if(++rowCount <= limitRows) {
        //Serial.println(" success line is " + jsonString);
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
  //Serial.println("out lst is" + finalString);
  DeserializationError err = deserializeJson(*resultDocument, finalString);
  if (err) {
    Serial.println("Not enough size or incorrect json document");
    return -1;
  }

  return rowCount;
}

int DBReadProcessor::queryJsonCount(String listName) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return -1;
  }
  File dataFile;
  String fileName = "/__quarkdb__/" +listName + +".jsonl";
  if (!SPIFFS.exists(fileName)) {
    // Serial.println("ERROR:List does not exist ->" + listName);
    return -1;
  }
  if (this->fileMode == 2) {
    dataFile = SPIFFS.open(fileName, "r");
  }
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



