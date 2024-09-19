#include "db_utils.h"
#include "filter_processor.h"
#include <ArduinoJson.h>

template<typename T>
bool filterLine(JsonVariant jsonCheckObject, JsonPair kv) {
  if (jsonCheckObject.is<T>() && kv.value().as<T>() == jsonCheckObject.as<T>()) {
    // Serial.println("match found at type");
    return false;
  } else {
    return true;
  }
}

DBFilterProcessor::DBFilterProcessor() {
  dbUtils = new DBUtils();
}

bool DBFilterProcessor::innerObjectParse(JsonPair kvx, JsonVariant jsonCheckObject) {
  bool innerMismatch = false;
  if (String(kvx.key().c_str()) == "$eq" && kvx.value().is<float>()) {
    innerMismatch = filterLine<float>(jsonCheckObject, kvx);
  } else if (String(kvx.key().c_str()) == "$eq" && kvx.value().is<String>()) {
    innerMismatch = filterLine<String>(jsonCheckObject, kvx);
  } else if (String(kvx.key().c_str()) == "$eq" && kvx.value().is<bool>()) {
    innerMismatch = filterLine<bool>(jsonCheckObject, kvx);
  } else if (String(kvx.key().c_str()) == "$gt" && kvx.value().is<float>()) {
    if (jsonCheckObject.is<float>() && kvx.value().as<float>() < jsonCheckObject.as<float>()) {
      // Serial.println("match found at $gt");
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$lt" && kvx.value().is<float>()) {
    if (jsonCheckObject.is<float>() && kvx.value().as<float>() > jsonCheckObject.as<float>()) {
      // Serial.println("match found at $lt");
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$gte" && kvx.value().is<float>()) {
    if (jsonCheckObject.is<float>() && kvx.value().as<float>() <= jsonCheckObject.as<float>()) {
      // Serial.println("match found at $gte");
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$lte" && kvx.value().is<float>()) {
    if (jsonCheckObject.is<float>() && kvx.value().as<float>() >= jsonCheckObject.as<float>()) {
      // Serial.println("match found at $lte");
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$eleMatch" && kvx.value().is<JsonObject>() && jsonCheckObject.is<JsonArray>()) {
    //  Serial.println("checking for array");
    for (JsonVariant arrayItem : jsonCheckObject.as<JsonArray>()) {
      bool testElement = filterParse(kvx.value().as<JsonObject>(), arrayItem, true);
      if (!testElement) {
        innerMismatch = false;
        break;
      } else {
        innerMismatch = testElement;
      }
    }
  } else {
    innerMismatch = true;
  }
  return innerMismatch;
}

bool DBFilterProcessor::filterParse(JsonObject rootFilter, JsonVariant jsonLineDocument, bool isArray) {
  bool mismatch = false;
  for (JsonPair kv : rootFilter) {
    //  Serial.println(" key is" + String(kv.key().c_str()));
    String keyStr = String(kv.key().c_str());
    JsonVariant jsonCheckObject;
    if (isArray && (keyStr == "$gt" || keyStr == "$gte" || keyStr == "$lt" || keyStr == "$lte" || keyStr == "$eq" || keyStr == "$eleMatch")) {
      jsonCheckObject = jsonLineDocument;
    } else {
      jsonCheckObject = navigateJson(jsonLineDocument, keyStr.c_str());
    }
    if (isArray && (keyStr == "$gt" || keyStr == "$gte" || keyStr == "$lt" || keyStr == "$lte" || keyStr == "$eq" || keyStr == "$eleMatch")) {
      mismatch = innerObjectParse(kv, jsonCheckObject);
    } else if (kv.value().is<float>()) {
      mismatch = filterLine<float>(jsonCheckObject, kv);
    } else if (kv.value().is<String>()) {
      mismatch = filterLine<String>(jsonCheckObject, kv);
    } else if (kv.value().is<bool>()) {
      mismatch = filterLine<bool>(jsonCheckObject, kv);
    } else if (kv.value().is<JsonObject>()) {
      bool innerMismatch = false;
      for (JsonPair kvx : kv.value().as<JsonObject>()) {
        innerMismatch = innerObjectParse(kvx, jsonCheckObject);
      }
      if (innerMismatch) {
        return true;
      }
    } else {
      mismatch = true;
    }
  }

  return mismatch;
}

JsonVariant DBFilterProcessor::navigateJson(JsonVariant root, const char* path) {
  JsonVariant current = root;
  String mainRefPath = String(path);
  char* token = strtok((char*)path, ".");
  bool valid = true;
  while (token != NULL && valid) {
    //Serial.println("checking with"+String(token));
    if (current.is<JsonObject>()) {
      current = current[token];
    } else if (current.is<JsonArray>() && dbUtils->isTokenDigit(token)) {
      int index = atoi(token);
      //Serial.println("checking in array index "+ String(index));
      current = current[index];
    } else {
      valid = false;
    }
    token = strtok(NULL, ".");
  }

  if (!valid) {
    DynamicJsonDocument doc(10);
    //Serial.println("current is empty");
    return doc;
  }
  //Serial.println("returnng current");
  //serializeJson(current, Serial);
  return current;
}