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

#include "db_utils.h"
#include <ArduinoJson.h>
#include <FS.h>
#if defined (ESP32)
#include <SPIFFS.h>
#endif


DBUtils::DBUtils() {

}

bool DBUtils::isTokenDigit(const char* instr) {
  if (strlen(instr) == 0) {
    return false;
  }
  for (size_t i = 0; i < strlen(instr); i++) {
    if (isdigit(instr[i])) {
      continue;
    }
    return false;
  }
  return true;
}


// File holding the records of a list
String DBUtils::getListFileName(String listName) {
  return "/__quarkdb__/" + listName + ".jsonl";
}

// Temp file a list is rewritten into. Only ever incomplete: once fully
// written it is renamed to the ".new" file
String DBUtils::getTempFileName(String listName) {
  return "/__quarkdb__/" + listName + ".tmp";
}

// Fully written replacement for the list file, waiting to be installed
String DBUtils::getNewFileName(String listName) {
  return "/__quarkdb__/" + listName + ".new";
}

// Replaces the list file with the ".new" file. SPIFFS rename fails when the
// target exists, so the list file is removed first.
bool DBUtils::installNewFile(String listName) {
  String fileName = getListFileName(listName);
  if (SPIFFS.exists(fileName) && !SPIFFS.remove(fileName)) {
    return false;
  }
  return SPIFFS.rename(getNewFileName(listName), fileName);
}

// Completes or rolls back a list rewrite interrupted by a power loss.
// A rewrite goes: write ".tmp" -> rename to ".new" -> remove ".jsonl" ->
// rename ".new" to ".jsonl". The file names alone tell what happened, without
// relying on whether a remove finished:
//  - ".tmp" is always incomplete and is discarded
//  - ".new" is always complete and replaces the list file
bool DBUtils::recoverList(String listName) {
  String tempFileName = getTempFileName(listName);
  if (SPIFFS.exists(tempFileName) && !SPIFFS.remove(tempFileName)) {
    return false;
  }
  if (SPIFFS.exists(getNewFileName(listName))) {
    return installNewFile(listName);
  }
  return true;
}

bool DBUtils::validateListName(const char* instr) {
  if(strlen(instr) == 0 || strlen(instr) > __QUARKDB_MAX_LIST_NAME_LEN__) {
    return false;
  }
  for (size_t i = 0; i < strlen(instr); i++) {
    if (isalpha(instr[i])) {
      continue;
    }
    if (isdigit(instr[i])) {
      continue;
    }
    if (instr[i] == '_') {
      continue;
    }
    if (instr[i] == '-') {
      continue;
    }
    return false;
  }
  return true;
}
