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

//This is the main interface class to be used for QuarkDB operations
#include "QuarkDB.h"
#include "reader/read_processor.h"
#include "writer/write_processor.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif

QuarkDB::QuarkDB() {
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
}
bool QuarkDB::init(byte fileMode) {
  this->fileMode = fileMode;
  if (this->fileMode == QUARKDB_SPIFFS_FILE_TYPE) {
    #if defined (ESP8266)
      SPIFFS.begin();
    #elif defined(ESP32)
     if(!SPIFFS.begin(true)){
         Serial.println("SPIFFS Mount Failed");
         return false;
     }
    #endif
  }else {
    Serial.println("ERROR: File mode not supported now");
    return false;
  }
  this->readProcessor = new DBReadProcessor(fileMode);
  this->writeProcessor = new DBWriteProcessor(fileMode);
  this->commandParser = new CommandParser(fileMode);
  commandParser->init(readProcessor, writeProcessor);
  return true;
}

// Sets the max records the lists in the DB can handle based on memory of the solution
void QuarkDB::setMaxRecords(int maxRecords) {
  this->maxRecords = maxRecords;
  readProcessor->setMaxRecords(maxRecords);
  writeProcessor->setMaxRecords(maxRecords);
  commandParser->setMaxRecords(maxRecords);
}

// Sets the max record size each record in the list in the DB can handle based on memory of the solution
void QuarkDB::setMaxRecordSize(int maxRecordSize) {
  this->maxRecordSize = maxRecordSize;
  readProcessor->setMaxRecordSize(maxRecordSize);
  writeProcessor->setMaxRecordSize(maxRecordSize);
  commandParser->setMaxRecordSize(maxRecordSize);
}

// Creates a list in the file system
bool QuarkDB::createList(String listName) {
  return writeProcessor->createList(listName);
}
// Deletes a list in the file system
bool QuarkDB::deleteList(String listName) {
  return writeProcessor->deleteList(listName);
}
// Gets total record count from the list
int QuarkDB::getRecordCount(String listName) {
  return readProcessor->getRecordCount(listName);
}
// Retrieves records as a JSON Array in the result document
int QuarkDB::getRecords(String listName, String selector, JsonDocument* resultDocument) {
    return readProcessor->getRecords(listName, selector, resultDocument, maxRecords);
}
// Retrieves records as a JSON Array in the result document. Limit using maximum rows to return
int QuarkDB::getRecords(String listName, String selector, JsonDocument* resultDocument , byte limitRows) {
  return readProcessor->getRecords(listName, selector, resultDocument, limitRows);
}
// Updates records as per the matching selector
int QuarkDB::updateRecords(String listName, String selector, JsonObject document) {
  return writeProcessor->updateRecords(listName, selector,&document);
}
// Deletes records as per the matching selector
int QuarkDB::deleteRecords(String listName, String selector) {
  return writeProcessor->deleteRecords(listName, selector);
}
// Add record as to the list
bool QuarkDB::addRecord(String listName, JsonObject document) {
  return this->writeProcessor->addRecord(listName, &document);
}
// Process command line interface through serial interface
void QuarkDB::processSerialCommand() {
  if (Serial.available() > 0) {
    String str = Serial.readStringUntil('\n');
    if (str.length() > 0) {
      str.trim();
      if (!str.startsWith("QUARKDB>", 0)) {
        Serial.println("ERROR:Invalid command. Add \"QUARKDB>\" at start");
      } else {
        Serial.println(this->commandParser->processCommand(str));
      }
    } else {
      Serial.println("ERROR:Invalid command. Empty command->" + str);
    }
  }
}

