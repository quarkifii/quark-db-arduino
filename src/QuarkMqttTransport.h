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

// QuarkTransport implementation on top of PubSubClient (https://github.com/knolleary/pubsubclient).
//
// This header is not included by QuarkDB.h so the library does not require
// PubSubClient unless the sketch uses cloud backup:
//
//   #include <PubSubClient.h>
//   #include <QuarkDB.h>
//   #include <QuarkMqttTransport.h>
//
//   WiFiClient wifi;
//   PubSubClient mqtt(wifi);
//   QuarkMqttTransport transport(mqtt);
//   QuarkSyncConfig syncConfig;
//   ...
//   mqtt.setBufferSize(syncConfig.maxPayloadBytes + 64);   // PubSubClient defaults to 256 bytes
//   mqtt.setCallback([](char* topic, uint8_t* payload, unsigned int length) {
//     quarkDB.handleMessage(topic, payload, length);
//   });
//   quarkDB.enableSync(&transport, syncConfig);
//   quarkDB.enableListSync("config");
//   ... after mqtt.connect(..): quarkDB.syncCloud();
#ifndef QUARK_MQTT_TRANSPORT_H
#define QUARK_MQTT_TRANSPORT_H
#include <Arduino.h>
#include <PubSubClient.h>
#include "QuarkTransport.h"

class QuarkMqttTransport : public QuarkTransport {
  private:
    PubSubClient* client;
    bool retained;
  public:
    // retained: publish sync messages with the MQTT retain flag
    explicit QuarkMqttTransport(PubSubClient& client, bool retained = false) {
      this->client = &client;
      this->retained = retained;
    }
    bool connected() override {
      return client->connected();
    }
    bool publish(const char* topic, const char* payload) override {
      return client->publish(topic, payload, retained);
    }
    bool subscribe(const char* topic) override {
      return client->subscribe(topic);
    }
    // What fits in the PubSubClient buffer next to the fixed header and topic
    size_t maxPayloadSize(const char* topic) override {
      size_t overhead = MQTT_MAX_HEADER_SIZE + 2 + strlen(topic);
      size_t bufferSize = client->getBufferSize();
      return bufferSize > overhead ? bufferSize - overhead : 0;
    }
    PubSubClient& getClient() {
      return *client;
    }
};

#endif
