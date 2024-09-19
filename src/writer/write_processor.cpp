#include "write_processor.h"
#include "db_utils.h"
#include "filter_processor.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>



DBWriteProcessor::DBWriteProcessor(byte fileMode) {
  this->fileMode = fileMode;
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
  dbUtils = new DBUtils();
  dbFilter = new DBFilterProcessor();
}
void DBWriteProcessor::init() {

}

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
  if (this->fileMode == 2) {
    file = SPIFFS.open(fileName, "w");
  }
  if (!file) {
    Serial.println("Error opening file for writing");
    return false;
  }
  file.close();
  return true;
}


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
  if (this->fileMode == 2) {
    SPIFFS.remove(fileName);
  }
  return true;
}

int DBWriteProcessor::updateRecords(String listName , String selector,JsonObject* jsonObject) {
  DynamicJsonDocument filterDoc(__QUARKDB_JSON_STRING_SIZE__);
  DeserializationError error = deserializeJson(filterDoc, selector);
  if (error) {
    Serial.println("ERROR: Invalid selector or size more than 500 bytes->" + selector);
    return -1;
  }
  String updateObjectJson;
  serializeJson((*jsonObject), updateObjectJson);
  return this->updateFromJson(listName, &filterDoc , updateObjectJson);
}

int DBWriteProcessor::updateFromJson(String listName, DynamicJsonDocument* filterDoc, String updateObjectJson) {
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
  String finalString = "";
  int updateCount = 0;
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
    if (this->fileMode == 2) {
      dataFile = SPIFFS.open(fileName, "w");
    }
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

int DBWriteProcessor::deleteRecords(String listName, String selector) {
 DynamicJsonDocument filterDoc(maxRecordSize);
  DeserializationError error = deserializeJson(filterDoc, selector);
  if (error) {
    Serial.println("ERROR: Invalid filter->" + selector);
    return false;
  }
  return this->deleteFromJson(listName, &filterDoc);
}

int DBWriteProcessor::deleteFromJson(String listName, DynamicJsonDocument* filterDoc) {
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
  String finalString = "";
  int deleteCount = 0;
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
    if (this->fileMode == 2) {
      dataFile = SPIFFS.open(fileName, "w");
    }
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
  if (this->fileMode == 2) {
    file = SPIFFS.open(fileName, "a");
  }
  if (!file) {
    Serial.println("Error opening file for writing");
    return false;
  }
  file.println(jsonString);
  Serial.println("writing record " +jsonString );
  file.close();
  return true;
}

bool DBWriteProcessor::removeAllLists() {
  return true;
}
