#include "quark_db.h"
#include "read_processor.h"
#include "write_processor.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>

QuarkDB::QuarkDB(byte fileMode) {
  this->fileMode = fileMode;
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
  this->readProcessor = new DBReadProcessor(fileMode);
  this->writeProcessor = new DBWriteProcessor(fileMode);
  this->commandParser = new CommandParser(fileMode);
}
void QuarkDB::init() {
  if (this->fileMode == QUARKDB_SPIFFS_FILE_TYPE) {
    SPIFFS.begin();
  }
  readProcessor->init();
  writeProcessor->init();
  commandParser->init(readProcessor, writeProcessor);
}
void QuarkDB::setMaxRecords(int maxRecords) {
  this->maxRecords = maxRecords;
  readProcessor->setMaxRecords(maxRecords);
  writeProcessor->setMaxRecords(maxRecords);
  commandParser->setMaxRecords(maxRecords);
}

void QuarkDB::setMaxRecordSize(int maxRecordSize) {
  this->maxRecordSize = maxRecordSize;
  readProcessor->setMaxRecordSize(maxRecordSize);
  writeProcessor->setMaxRecordSize(maxRecordSize);
  commandParser->setMaxRecordSize(maxRecordSize);
}

bool QuarkDB::createList(String listName) {
  return writeProcessor->createList(listName);
}

bool QuarkDB::deleteList(String listName) {
  return writeProcessor->deleteList(listName);
}

int QuarkDB::getRecordCount(String listName) {
  return readProcessor->getRecordCount(listName);
}

int QuarkDB::getRecords(String listName, String selector, DynamicJsonDocument* resultDocument) {
    return readProcessor->getRecords(listName, selector, resultDocument, maxRecords);
}

int QuarkDB::getRecords(String listName, String selector, DynamicJsonDocument* resultDocument , byte limitRows) {
  return readProcessor->getRecords(listName, selector, resultDocument, limitRows);
}

int QuarkDB::updateRecords(String listName, String selector, JsonObject document) {
  return writeProcessor->updateRecords(listName, selector,&document);
}

int QuarkDB::deleteRecords(String listName, String selector) {
  return writeProcessor->deleteRecords(listName, selector);
}

bool QuarkDB::addRecord(String listName, JsonObject document) {
  return this->writeProcessor->addRecord(listName, &document);
}

void QuarkDB::processSerialCommand() {
  if (Serial.available() > 0) {
    String str = Serial.readStringUntil('\n');
    if (str.length() > 0) {
      str.trim();
      if (!str.startsWith("QUARKDB>", 0)) {
        Serial.println("Invalid command");
      } else {
        Serial.println(this->commandParser->processCommand(str));
      }
    } else {
      Serial.println("Invalid commandb->" + str);
    }
  }
}

