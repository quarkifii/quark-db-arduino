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
