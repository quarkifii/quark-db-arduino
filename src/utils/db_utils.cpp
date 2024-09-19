#include "db_utils.h"
#include <ArduinoJson.h>


DBUtils::DBUtils() {

}

bool DBUtils::isTokenDigit(const char* instr) {
  if (strlen(instr) == 0) {
    return false;
  }
  for (int i = 0; i < strlen(instr); i++) {
    if (isdigit(instr[i])) {
      continue;
    }
    return false;
  }
  return true;
}


bool DBUtils::validateListName(const char* instr) {
  if(strlen(instr) == 0 ) {
    return false;
  }
  for (int i = 0; i < strlen(instr); i++) {
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
