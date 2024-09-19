#ifndef QUARK_DB_COMMAND_PARSER_H
#define QUARK_DB_COMMAND_PARSER_H
#include "read_processor.h"
#include "write_processor.h"
#include "db_utils.h"
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
