#ifndef QUARK_DB_UTIL_READ_H
#define QUARK_DB_UTIL_READ_H
#define __QUARKDB_JSON_STRING_SIZE__ 500
#define __QUARKDB_FILTER_SIZE__  500
#define __QUARKDB_UPDATE_CMD_SIZE__  700
#define __QUARKDB_MAX_RECORDS__  300
#define __QUARKDB_MAX_RECORD_SIZE__  500
#define QUARKDB_SPIFFS_FILE_TYPE  2

#include <Arduino.h>
#include <ArduinoJson.h>

class DBUtils {
  public:
    DBUtils();
    bool validateListName(const char* instr);
    bool isTokenDigit(const char* instr);
};

#endif
