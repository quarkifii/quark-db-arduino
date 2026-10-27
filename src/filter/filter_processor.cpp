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

// Json filter implementation
#include "../utils/db_utils.h"
#include "filter_processor.h"
#include <ArduinoJson.h>

template<typename T>
bool filterLine(JsonVariant jsonCheckObject, JsonPair kv) {
  if (jsonCheckObject.is<T>() && kv.value().as<T>() == jsonCheckObject.as<T>()) {
    return false;
  } else {
    return true;
  }
}

DBFilterProcessor::DBFilterProcessor() {
  dbUtils = new DBUtils();
}

//Parse internal object in recursive support
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
      innerMismatch = false;
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$lt" && kvx.value().is<float>()) {
    if (jsonCheckObject.is<float>() && kvx.value().as<float>() > jsonCheckObject.as<float>()) {
      innerMismatch = false;
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$gte" && kvx.value().is<float>()) {
    if (jsonCheckObject.is<float>() && kvx.value().as<float>() <= jsonCheckObject.as<float>()) {
      innerMismatch = false;
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$lte" && kvx.value().is<float>()) {
    if (jsonCheckObject.is<float>() && kvx.value().as<float>() >= jsonCheckObject.as<float>()) {
      innerMismatch = false;
    } else {
      innerMismatch = true;
    }
  } else if (String(kvx.key().c_str()) == "$eleMatch" && kvx.value().is<JsonObject>() && jsonCheckObject.is<JsonArray>()) {
    // No element matched yet, so an empty array is a mismatch
    innerMismatch = true;
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

//Parse filter to go recursively inside document to scan and search
//Every key of the filter must match (AND); returns true on the first mismatch
bool DBFilterProcessor::filterParse(JsonObject rootFilter, JsonVariant jsonLineDocument, bool isArray) {
  for (JsonPair kv : rootFilter) {
    String keyStr = String(kv.key().c_str());
    bool isOperator = isArray && (keyStr == "$gt" || keyStr == "$gte" || keyStr == "$lt" || keyStr == "$lte" || keyStr == "$eq" || keyStr == "$eleMatch");
    JsonVariant jsonCheckObject;
    if (isOperator) {
      jsonCheckObject = jsonLineDocument;
    } else {
      jsonCheckObject = navigateJson(jsonLineDocument, keyStr.c_str());
    }
    bool mismatch = false;
    if (isOperator) {
      mismatch = innerObjectParse(kv, jsonCheckObject);
    } else if (kv.value().is<float>()) {
      mismatch = filterLine<float>(jsonCheckObject, kv);
    } else if (kv.value().is<String>()) {
      mismatch = filterLine<String>(jsonCheckObject, kv);
    } else if (kv.value().is<bool>()) {
      mismatch = filterLine<bool>(jsonCheckObject, kv);
    } else if (kv.value().is<JsonObject>()) {
      //Every operator on the field must match, e.g. {"$gt" : 1, "$lt" : 5}
      for (JsonPair kvx : kv.value().as<JsonObject>()) {
        if (innerObjectParse(kvx, jsonCheckObject)) {
          mismatch = true;
          break;
        }
      }
    } else {
      mismatch = true;
    }
    if (mismatch) {
      return true;
    }
  }

  return false;
}
// Navigate in the object to get the required value
JsonVariant DBFilterProcessor::navigateJson(JsonVariant root, const char* path) {
  JsonVariant current = root;
  // Tokenize a copy so the caller's path string is not modified
  String mainRefPath = String(path);
  char* token = strtok((char*)mainRefPath.c_str(), ".");
  bool valid = true;
  while (token != NULL && valid) {
    if (current.is<JsonObject>()) {
      current = current[token];
    } else if (current.is<JsonArray>() && dbUtils->isTokenDigit(token)) {
      int index = atoi(token);
      current = current[index];
    } else {
      valid = false;
    }
    token = strtok(NULL, ".");
  }

  if (!valid) {
    // Null variant; must not point into a local JsonDocument destroyed on return
    return JsonVariant();
  }
  return current;
}