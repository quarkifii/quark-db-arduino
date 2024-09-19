#include "read_processor.h"
#include "write_processor.h"
#include "command_parser.h"
#include "db_utils.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>

CommandParser::CommandParser(byte mode) {
  this->fileMode = fileMode;
  this->chipSelect = chipSelect;
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
  dbUtils = new DBUtils();
}

void CommandParser::init(DBReadProcessor* readProcessor, DBWriteProcessor* writeProcessor) {
  this->readProcessor = readProcessor;
  this->writeProcessor = writeProcessor;
}
String CommandParser::processCommand(String cmd) {
  String mainCommand = cmd.substring(8);
  String mainRefCommand = String(mainCommand);
  mainRefCommand.trim();
  if (mainRefCommand.startsWith("set max_records=")) {
    String maxRecordsIn = mainRefCommand.substring(mainRefCommand.indexOf("=") + 1);
    maxRecordsIn.trim();
    if (dbUtils->isTokenDigit(maxRecordsIn.c_str())) {
      int maxRecordsInt = atoi(maxRecordsIn.c_str());
      this->maxRecords = maxRecordsInt;
      return " Successfully set";
    } else {
      return "Invalid value";
    }
  }
  if (mainRefCommand.startsWith("set max_record_size=")) {
    String maxRecordSizeIn = mainRefCommand.substring(mainRefCommand.indexOf("=") + 1);
    maxRecordSizeIn.trim();
    if (dbUtils->isTokenDigit(maxRecordSizeIn.c_str())) {
      int maxRecordSizeInt = atoi(maxRecordSizeIn.c_str());
      this->maxRecordSize = maxRecordSizeInt;
      return " Successfully set";
    } else {
      return "Invalid value";
    }
  }
  if (mainRefCommand.startsWith("show lists")) {
    Dir dir = SPIFFS.openDir("/");
    String lists = "Lists found are..";
    while (dir.next()) {
      if (dir.fileName().startsWith("/__quarkdb__")) {
        lists = lists + "\n" + dir.fileName().substring(dir.fileName().lastIndexOf("/") + 1, dir.fileName().lastIndexOf("."));
      }
    }
    if(lists == "Lists found are..") {
      return " No Lists Found";
    }
    return lists;
  }
  char* token = strtok((char*)mainCommand.c_str(), ".");
  short tokenCount = 0;
  bool isCreate = false;
  bool isDelete = false;
  String listName = "";
  while (token != NULL) {
    //Serial.println(token);

    String tokStr = String(token);
    tokStr.trim();
    //Serial.println("token str is" + tokStr);
    if (tokenCount == 0 and tokStr != "db") {
      return "Invalid command -> " + tokStr;
    }
    if (tokenCount == 1 && tokStr.length() == 0) {
      return "Invalid command -> Empty db";
    }
    if (tokenCount == 1 && tokStr.length() > 0 && tokStr.startsWith("create", 0)) {
      String lName = preProcessCommand(tokStr);
      if (!lName.startsWith("ERROR:")) {
        return processCreate(lName);
      }
      return lName;
      //return handleListCreate(tokStr);
      // return "Command -> Create List";
    }
    if (tokenCount == 1 && tokStr.length() > 0 && tokStr.startsWith("delete", 0)) {
      String lName = preProcessCommand(tokStr);
      if (!lName.startsWith("ERROR:")) {
        return processDelete(lName);
      }
      return lName;
    }
    if (tokenCount == 1 && tokStr.length() > 0) {
      listName = tokStr;
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("add", 0)) {
      String commandList = mainRefCommand.substring(mainRefCommand.indexOf("(") + 1, mainRefCommand.lastIndexOf(")"));
      commandList.trim();
      String json = preProcessListCommand(listName, commandList);
      if (!json.startsWith("ERROR:")) {
        return processAdd(listName, json);
      }
      return json;
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("find", 0)) {
      String commandList = mainRefCommand.substring(mainRefCommand.indexOf("(") + 1, mainRefCommand.lastIndexOf(")"));
      commandList.trim();
      String json = preProcessListCommand(listName, commandList);
      if (!json.startsWith("ERROR:")) {
        return processGet(listName, json);
      }
      return json;
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("count", 0)) {
       return processGetCount(listName);
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("update", 0)) {
      String commandList = mainRefCommand.substring(mainRefCommand.indexOf("(") + 1, mainRefCommand.lastIndexOf(")"));
      commandList.trim();
      String json = preProcessListCommand(listName, commandList);
      if (!json.startsWith("ERROR:")) {
        return processSet(listName, json);
      }
      return json;
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("delete", 0)) {
      String commandList = mainRefCommand.substring(mainRefCommand.indexOf("(") + 1, mainRefCommand.lastIndexOf(")"));
      commandList.trim();
      String json = preProcessListCommand(listName, commandList);
      if (!json.startsWith("ERROR:")) {
        return processDelete(listName, json);
      }
      return json;
    }
    tokenCount++;
    if (tokenCount == 3) {
      break;
    }
    token = strtok(NULL, ".");
  }


  return "Invalid Command -> " + cmd.substring(8);
}

//QUARKDB>db.hhg.vvf testTEST_- test
//QUARKDB>db.test.find({"test" : 22})

String CommandParser::preProcessCommand(String command) {
  String listName = command.substring(command.indexOf("(") + 1, command.lastIndexOf(")"));
  listName.trim();
  if (!listName.startsWith("\"") || listName.lastIndexOf("\"") != listName.length() - 1) {
    return "ERROR:Enclose list name in double quotes(\")";
  }
  String fList = listName.substring(listName.indexOf("\"") + 1, listName.lastIndexOf("\""));
  fList.trim();
  if (fList.length() == 0) {
    return "ERROR:Empty List Name ->" + command;
  }
  return fList;
}

String CommandParser::preProcessListCommand(String listName, String commandList) {

  //Serial.println("commandlist is" + commandList);
  if (!commandList.startsWith("{") || commandList.lastIndexOf("}") != commandList.length() - 1) {
    return "ERROR:Add valid json object with {}";
  }
  String json = commandList.substring(commandList.indexOf("{"), commandList.lastIndexOf("}")) + "}";
  json.trim();

  return json;
}

String CommandParser::processGet(String listName, String filter) {
  DynamicJsonDocument doc(__QUARKDB_FILTER_SIZE__);
  DeserializationError error = deserializeJson(doc, filter);
  if (error) {
    return "ERROR: Invalid filter->" + filter;
  }
  int count = readProcessor->getRecordCount(listName);
  if (count == -1) {
    return "ERROR: Error reading list->" + listName;
  }
  int allowedCount = 0;
  if(count <= maxRecords) {
    allowedCount = count;
  }else {
    allowedCount = maxRecords;
  }
  DynamicJsonDocument resultDocument(allowedCount * maxRecordSize);
  int total = readProcessor->getRecords(listName, filter, &resultDocument, maxRecords);
  if (total == -1) {
    return "ERROR: Could not read list";
  }
  serializeJsonPretty(resultDocument, Serial);
  Serial.println("");
  int resCount = total;
  if(total > maxRecords) {
     resCount = maxRecords;
  }
  return String(resCount) +"/" + String(total) + " Records Retrieved [max records set : " + String(maxRecords) + " bytes, max single record size set : " + String(maxRecordSize) + "bytes ]";
}

String CommandParser::processGetCount(String listName) {
  int status = readProcessor->getRecordCount(listName);
  if (status == -1) {
    return "ERROR: Could not read list";
  }
  return "Total Count -> " + String(status) + " [max records set : " + String(maxRecords) + ", max single record size set : " + String(maxRecordSize) + " ]";
}

String CommandParser::processDelete(String listName, String filter) {
  DynamicJsonDocument doc(__QUARKDB_FILTER_SIZE__);
  DeserializationError error = deserializeJson(doc, filter);
  if (error) {
    return "ERROR: Invalid filter->" + filter;
  }

  int status = writeProcessor->deleteRecords(listName, filter);
  if (status == -1) {
    return "ERROR: Could not delete records";
  }
  return "Results Deleted Count [" + String(status) +   "]";
}

String CommandParser::processSet(String listName, String updateCommand) {
  DynamicJsonDocument doc(__QUARKDB_UPDATE_CMD_SIZE__);
  DeserializationError error = deserializeJson(doc, updateCommand);
  if (error) {
    return "ERROR: Invalid updateCommand. Please pass valid JSON string->" + updateCommand;
  }

  JsonObject filter = doc["filter"];
  JsonObject updateObj = doc["updateObj"];
  if(filter == NULL || updateObj == NULL) {
    return "ERROR: Invalid updateCommand . Please pass filter and updateObj ->" + updateCommand;
  }
  String filterStr;
  serializeJson(filter, filterStr);
  int status = writeProcessor->updateRecords(listName, filterStr,&updateObj);
  if (status == -1) {
    return "ERROR: Could not update list";
  }
   return "Results Updated Count [" + String(status)  +  "]";
}

String CommandParser::processAdd(String listName, String json) {
  DynamicJsonDocument doc(maxRecordSize);
  DeserializationError error = deserializeJson(doc, json);
  if (error) {
    return "ERROR: Invalid json->" + json;
  }
  JsonObject jsonObj = doc.as<JsonObject>();

  bool status = writeProcessor->addRecord(listName, &jsonObj);
  if (status) {
    return "Successfully added to ->" + listName;
  } else {
    return "Failed to add into ->" + listName;
  }
}

String CommandParser::processCreate(String listName) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return "ERROR:Non Empty alphanumeric , _ , - characters allowed only ->" + listName;
  }
  bool status = writeProcessor->createList(listName);
  if (status) {
    return "Successfully created ->" + listName;
  } else {
    return "Failed to create ->" + listName;
  }
}

String CommandParser::processDelete(String listName) {
  listName.trim();
  if (!dbUtils->validateListName(listName.c_str())) {
    return "ERROR:Non Empty alphanumeric , _ , - characters allowed only ->" + listName;
  }
  bool status = writeProcessor->deleteList(listName);
  if (status) {
    return "Successfully deleted ->" + listName;
  } else {
    return "Failed to delete ->" + listName;
  }
}
