#ifndef QUARK_DB_WRITE_H
#define QUARK_DB_WRITE_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include "db_utils.h"
#include "filter_processor.h"

class DBWriteProcessor {
  
  private:
    byte fileMode;
    int chipSelect;
    int maxRecords;
    int maxRecordSize;
    String databaseName;
    DBUtils* dbUtils; 
    DBFilterProcessor* dbFilter; 
    int updateFromJson(String listName, DynamicJsonDocument* filterDoc, String updateObjectJson);
    int deleteFromJson(String listName, DynamicJsonDocument* filterDoc);
  public:
    DBWriteProcessor(byte fileMode);
    void init();
    bool createList(String listName);
    bool deleteList(String listName);
    int updateRecords(String listName , String selector,JsonObject* jsonObject);
    int deleteRecords(String listName , String selector);
    bool addRecord(String listName, JsonObject* jsonObject);
    bool removeAllLists();
    void setMaxRecords(int maxRecords);
    void setMaxRecordSize(int maxRecords);          
};

#endif
