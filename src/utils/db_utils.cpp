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

// Temp file a list is rewritten into before it replaces the list file
String DBUtils::getTempFileName(String listName) {
  return "/__quarkdb__/" + listName + ".tmp";
}

// Completes or rolls back a list rewrite interrupted by a power loss.
// The temp file is only swapped in after it is fully written, so:
//  - list file and temp file present: the rewrite never finished, the list
//    file is intact and the temp file is discarded
//  - only the temp file present: power was lost between removing the list
//    file and renaming the complete temp file, so the rename is finished
bool DBUtils::recoverList(String listName) {
  String tempFileName = getTempFileName(listName);
  if (!SPIFFS.exists(tempFileName)) {
    return true;
  }
  String fileName = getListFileName(listName);
  if (SPIFFS.exists(fileName)) {
    return SPIFFS.remove(tempFileName);
  }
  return SPIFFS.rename(tempFileName, fileName);
}

bool DBUtils::validateListName(const char* instr) {
  if(strlen(instr) == 0 ) {
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
