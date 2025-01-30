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
bool DBFilterProcessor::filterParse(JsonObject rootFilter, JsonVariant jsonLineDocument, bool isArray) {
  bool mismatch = false;
  for (JsonPair kv : rootFilter) {
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
// Navigate in the object to get the required value
JsonVariant DBFilterProcessor::navigateJson(JsonVariant root, const char* path) {
  JsonVariant current = root;
  String mainRefPath = String(path);
  char* token = strtok((char*)path, ".");
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
    JsonDocument doc;
    return doc;
  }
  return current;
}