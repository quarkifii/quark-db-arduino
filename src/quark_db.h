#ifndef QUARK_DB_H
#define QUARK_DB_H

#include "read_processor.h"
#include "write_processor.h"
#include "command_parser.h"
#include <Arduino.h>
#include <ArduinoJson.h>


class QuarkDB {
  
  private:
    byte fileMode;
    int chipSelect;
    int maxRecords;
    int maxRecordSize;
    String databaseName;
    DBReadProcessor* readProcessor;
    DBWriteProcessor* writeProcessor;
    CommandParser* commandParser;
  public:
    QuarkDB(byte mode);
    void init();
    bool createList(String listName);
    void processSerialCommand();
    bool deleteList(String listName);
    int getRecordCount(String listName);
    int getRecords(String listName, String selector, DynamicJsonDocument* resultDocument);
    int getRecords(String listName, String selector, DynamicJsonDocument* resultDocument , byte limitRows);
    int updateRecords(String listName , String selector,JsonObject document);
    int deleteRecords(String listName , String selector);
    bool addRecord(String listName, JsonObject document);
    bool removeRecords(String listName); 
    bool removeAllLists(); 
    void setMaxRecords(int maxRecords);
    void setMaxRecordSize(int maxRecordSize);       
    DynamicJsonDocument * getDBStat();
};

#endif
