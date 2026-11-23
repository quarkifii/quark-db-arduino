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

// Observer for successful writes. DBWriteProcessor reports every change made
// through the API or the serial CLI so the sync layer can log it to the
// oplog. Callbacks run after the list file has been updated.
#ifndef QUARK_DB_WRITE_LISTENER_H
#define QUARK_DB_WRITE_LISTENER_H
#include <Arduino.h>

class DBWriteListener {
  public:
    virtual ~DBWriteListener() {}
    virtual void onRecordAdded(const String& listName, const String& docJson) = 0;
    virtual void onRecordsUpdated(const String& listName, const String& selectorJson, const String& docJson, int count) = 0;
    virtual void onRecordsDeleted(const String& listName, const String& selectorJson, int count) = 0;
    virtual void onListDeleted(const String& listName) = 0;
    virtual void onListCreated(const String& listName) = 0;
};

#endif
