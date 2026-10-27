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


//CLI implementation
//Examples in serial interface
//QUARKDB>db.create("test")
//QUARKDB>db.test.add({"test" : 22})
//QUARKDB>db.test.find({"test" : 22})
//QUARKDB>db.delete("test")
#include "../reader/read_processor.h"
#include "../writer/write_processor.h"
#include "command_parser.h"
#include "../utils/db_utils.h"
#include <ArduinoJson.h>
#include <SPI.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif

CommandParser::CommandParser(byte mode) {
  this->fileMode = fileMode;
  this->chipSelect = chipSelect;
  this->maxRecords = __QUARKDB_MAX_RECORDS__;
  this->maxRecordSize = __QUARKDB_MAX_RECORD_SIZE__;
  dbUtils = new DBUtils();
}

//Initialize
void CommandParser::init(DBReadProcessor* readProcessor, DBWriteProcessor* writeProcessor) {
  this->readProcessor = readProcessor;
  this->writeProcessor = writeProcessor;
}
String sterilizeCommandPart(String tokStr) {
 String fullCommand = tokStr.substring(tokStr.indexOf("("));
 if(!fullCommand.startsWith("(") || fullCommand.lastIndexOf(")") != fullCommand.length() - 1) {
  return "ERROR:Use valid command";
 }
 return "";
}
String sterilizeCommandList(String tokStr , String mainRefCommand) {
 String error = sterilizeCommandPart(tokStr);
 if(error != "") {
  return error;
 }
 String commandList = mainRefCommand.substring(mainRefCommand.indexOf("(") + 1, mainRefCommand.lastIndexOf(")"));
 commandList.trim();
 return commandList;
}


//Process command to parse out different types of commands
String CommandParser::processCommand(String cmd) {
  String coreCmd = cmd.substring(8);
  String mainCommand = coreCmd.substring(0,coreCmd.indexOf("("));
  String mainRefCommand = String(coreCmd);
  mainRefCommand.trim();
  if (mainRefCommand.startsWith("set max_records=")) {
    String maxRecordsIn = mainRefCommand.substring(mainRefCommand.indexOf("=") + 1);
    maxRecordsIn.trim();
    if (dbUtils->isTokenDigit(maxRecordsIn.c_str())) {
      int maxRecordsInt = atoi(maxRecordsIn.c_str());
      this->maxRecords = maxRecordsInt;
      return " Successfully set";
    } else {
      return "ERROR:Invalid value";
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
      return "ERROR:Invalid value";
    }
  }
  if (mainRefCommand.startsWith("show lists")) {
    return readProcessor->getListNames();
  }
  char* token = strtok((char*)mainCommand.c_str(), ".");
  short tokenCount = 0;
  String listName = "";
  while (token != NULL) {
    String tokStr = String(token);
    tokStr.trim();
    if (tokenCount == 0 and tokStr != "db") {
      return "ERROR:Invalid command -> " + tokStr;
    }
    if (tokenCount == 1 && tokStr.length() == 0) {
      return "ERROR:Invalid command -> Empty db";
    }
    if (tokenCount == 1 && tokStr.length() > 0 && tokStr.startsWith("create", 0)) {
      String fullCommand = sterilizeCommandPart(coreCmd);
      if(fullCommand != "") return fullCommand;
      String lName = preProcessCommand(coreCmd);
      if (!lName.startsWith("ERROR:")) {
        return processCreate(lName);
      }
      return lName;
    }
    if (tokenCount == 1 && tokStr.length() > 0 && tokStr.startsWith("delete", 0)) {
      String fullCommand = sterilizeCommandPart(coreCmd);
      if(fullCommand != "") return fullCommand;
      String lName = preProcessCommand(coreCmd);
      if (!lName.startsWith("ERROR:")) {
        return processDelete(lName);
      }
      return lName;
    }
    if (tokenCount == 1 && tokStr.length() > 0) {
      listName = tokStr;
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("add", 0)) {
      String commandList = sterilizeCommandList(coreCmd , mainRefCommand);
      if (commandList.startsWith("ERROR:")) return commandList;
      String json = preProcessListCommand(listName, commandList);
      if (!json.startsWith("ERROR:")) {
        return processAdd(listName, json);
      }
      return json;
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("find", 0)) {
      String commandList = sterilizeCommandList(coreCmd , mainRefCommand);
      if (commandList.startsWith("ERROR:")) return commandList;
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
      String commandList = sterilizeCommandList(coreCmd , mainRefCommand);
      if (commandList.startsWith("ERROR:")) return commandList;
      String json = preProcessListCommand(listName, commandList);
      if (!json.startsWith("ERROR:")) {
        return processSet(listName, json);
      }
      return json;
    }
    if (tokenCount == 2 && tokStr.length() > 0 && tokStr.startsWith("delete", 0)) {
      String commandList = sterilizeCommandList(coreCmd , mainRefCommand);
      if (commandList.startsWith("ERROR:")) return commandList;
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

// Preprocess command to create and delete list
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

// Preprocess command to perform list specific operations
String CommandParser::preProcessListCommand(String listName, String commandList) {
  if (!commandList.startsWith("{") || commandList.lastIndexOf("}") != commandList.length() - 1) {
    return "ERROR:Add valid json object with {}";
  }
  String json = commandList.substring(commandList.indexOf("{"), commandList.lastIndexOf("}")) + "}";
  json.trim();
  return json;
}

// Process list get from the db
String CommandParser::processGet(String listName, String filter) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, filter);
  if (error) {
    return "ERROR: Invalid filter->" + filter;
  }
  int count = readProcessor->getRecordCount(listName);
  if (count == -1) {
    return "ERROR: Error reading list->" + listName;
  }

  JsonDocument resultDocument;
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
// Process list count from the db
String CommandParser::processGetCount(String listName) {
  int status = readProcessor->getRecordCount(listName);
  if (status == -1) {
    return "ERROR: Could not read list";
  }
  return "Total Count -> " + String(status) + " [max records set : " + String(maxRecords) + ", max single record size set : " + String(maxRecordSize) + " ]";
}
// Process list delete from the db
String CommandParser::processDelete(String listName, String filter) {
  JsonDocument doc;
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
// Process list update in the db
String CommandParser::processSet(String listName, String updateCommand) {
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, updateCommand);
  if (error) {
    return "ERROR: Invalid updateCommand. Please pass valid JSON string->" + updateCommand;
  }

  JsonObject filter = doc["filter"];
  JsonObject updateObj = doc["updateObj"];
  if(filter.isNull() || updateObj.isNull()) {
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
// Process list add element to the db
String CommandParser::processAdd(String listName, String json) {
  JsonDocument doc;
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
// Process list addition in the db
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
// Process list deletion from the db
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


