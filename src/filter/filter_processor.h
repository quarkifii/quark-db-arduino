#ifndef QUARK_DB_FILTER_H
#define QUARK_DB_FILTER_H
#include <Arduino.h>
#include <ArduinoJson.h>
#include "db_utils.h"

class DBFilterProcessor {
  private:
    DBUtils* dbUtils; 
    bool innerObjectParse(JsonPair kvx,JsonVariant jsonCheckObject);
    JsonVariant navigateJson(JsonVariant root, const char* path);
  public:
    DBFilterProcessor();
    bool filterParse(JsonObject rootFilter , JsonVariant jsonLineDocument ,bool isArray);
};

#endif
