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

// Transport QuarkDB publishes through and receives from. It is the boundary
// between the database and whatever carries the messages (an MQTT client,
// a serial link, a test double). QuarkMqttTransport.h implements it on top
// of PubSubClient; implement it yourself for any other client.
#ifndef QUARK_TRANSPORT_H
#define QUARK_TRANSPORT_H
#include <Arduino.h>

class QuarkTransport {
  public:
    virtual ~QuarkTransport() {}
    // True while the connection to the broker is up
    virtual bool connected() = 0;
    // Publish a NUL terminated payload. Returns false if it could not be sent.
    virtual bool publish(const char* topic, const char* payload) = 0;
    // Subscribe to the topic the cloud answers on
    virtual bool subscribe(const char* topic) = 0;
    // Largest payload that fits in a single message on this topic, or 0 if
    // the transport does not know (then only QuarkSyncConfig::maxPayloadBytes applies)
    virtual size_t maxPayloadSize(const char* /*topic*/) { return 0; }
};

#endif
