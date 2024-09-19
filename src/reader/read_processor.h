#ifndef QUARK_DB_READ_H
#define QUARK_DB_READ_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include "db_utils.h"
#include "filter_processor.h"

class DBReadProcessor {
  private:
    byte fileMode;
    int chipSelect;
    int maxRecords;
    int maxRecordSize;
    DBUtils* dbUtils; 
    DBFilterProcessor* dbFilter; 
    bool filterParse(JsonObject rootFilter , JsonVariant jsonLineDocument ,bool isArray);
    int queryJson(String listName, DynamicJsonDocument* filterDoc, DynamicJsonDocument* resultDocument, int limitRows);
    bool innerObjectParse(JsonPair kvx,JsonVariant jsonCheckObject);
    int queryJsonCount(String listName);
  public:
    DBReadProcessor(byte fileMode);
    void init();
    int getRecordCount(String listName);
    int getRecords(String listName, String selector, DynamicJsonDocument* resultDoc , int limitRows);
    void setMaxRecords(int maxRecords);
    void setMaxRecordSize(int maxRecords);  
};

#endif
