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

// Interface for command parser in CLI
#ifndef QUARK_DB_COMMAND_PARSER_H
#define QUARK_DB_COMMAND_PARSER_H
#include "../reader/read_processor.h"
#include "../writer/write_processor.h"
#include "../utils/db_utils.h"
#include <Arduino.h>

class CommandParser {
  private:
    byte fileMode;
    int chipSelect;
    int maxRecords;
    int maxRecordSize;
    String lastCommand;
    DBUtils* dbUtils; 
    DBReadProcessor* readProcessor;
    DBWriteProcessor* writeProcessor;
    String preProcessCommand(String command);
    String preProcessListCommand(String listName,String command);
    String processGet(String listName , String filter);
    String processGetCount(String listName);
    String processAdd(String listName , String jsonString);
    String processDelete(String listName, String filter);
    String processSet(String listName , String updateCommand);
    String processCreate(String listName);
    String processDelete(String listName);
  public:
    CommandParser(byte mode);
    void init(DBReadProcessor* readProcessor , DBWriteProcessor* writeProcessor);
    String processCommand(String command);
    void setMaxRecords(int maxRecords);
    void setMaxRecordSize(int maxRecords);  
};

#endif
