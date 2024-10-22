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

#ifndef QUARK_DB_H
#define QUARK_DB_H

#include "reader/read_processor.h"
#include "writer/write_processor.h"
#include "command/command_parser.h"
#include <Arduino.h>
#include <ArduinoJson.h>


class QuarkDB {
  
  private:
    byte fileMode;
    int maxRecords;
    int maxRecordSize;
    String databaseName;
    DBReadProcessor* readProcessor;
    DBWriteProcessor* writeProcessor;
    CommandParser* commandParser;
  public:
    QuarkDB();
    bool init(byte mode);
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
