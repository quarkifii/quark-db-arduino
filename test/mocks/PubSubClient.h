// The MIT License (MIT)
//
// Copyright (c) 2024-2026 Quarkifi Technologies Pvt Ltd
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

// Host-side mock of PubSubClient (knolleary/pubsubclient 2.8) used by the
// cloud backup tests. Same public API as the real client; nothing touches
// the network. Publishes are recorded, the buffer-size rule of the real
// publish() is enforced, and tests inject incoming messages and connection
// state with the helpers at the bottom.
#ifndef QUARKDB_MOCK_PUBSUBCLIENT_H
#define QUARKDB_MOCK_PUBSUBCLIENT_H

#include <Arduino.h>
#include <Client.h>

#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <vector>

#define MQTT_VERSION 4
#define MQTT_MAX_PACKET_SIZE 256
#define MQTT_KEEPALIVE 15
#define MQTT_SOCKET_TIMEOUT 15
#define MQTT_MAX_HEADER_SIZE 5

#define MQTT_CONNECTION_TIMEOUT -4
#define MQTT_CONNECTION_LOST -3
#define MQTT_CONNECT_FAILED -2
#define MQTT_DISCONNECTED -1
#define MQTT_CONNECTED 0
#define MQTT_CONNECT_BAD_PROTOCOL 1
#define MQTT_CONNECT_BAD_CLIENT_ID 2
#define MQTT_CONNECT_UNAVAILABLE 3
#define MQTT_CONNECT_BAD_CREDENTIALS 4
#define MQTT_CONNECT_UNAUTHORIZED 5

#define MQTT_CALLBACK_SIGNATURE std::function<void(char*, uint8_t*, unsigned int)> callback

class PubSubClient : public Print {
 public:
  struct Message {
    std::string topic;
    std::string payload;
    bool retained;
  };

  PubSubClient() {}
  explicit PubSubClient(Client& client) : client_(&client) {}
  PubSubClient(const char* domain, uint16_t port, Client& client) : client_(&client), domain_(domain), port_(port) {}
  PubSubClient(const char* domain, uint16_t port, MQTT_CALLBACK_SIGNATURE, Client& client)
      : client_(&client), domain_(domain), port_(port), callback_(callback) {}

  // ---- real PubSubClient API ----
  PubSubClient& setServer(const char* domain, uint16_t port) { domain_ = domain; port_ = port; return *this; }
  PubSubClient& setCallback(MQTT_CALLBACK_SIGNATURE) { callback_ = callback; return *this; }
  PubSubClient& setClient(Client& client) { client_ = &client; return *this; }
  PubSubClient& setKeepAlive(uint16_t keepAlive) { keepAlive_ = keepAlive; return *this; }
  PubSubClient& setSocketTimeout(uint16_t timeout) { socketTimeout_ = timeout; return *this; }
  boolean setBufferSize(uint16_t size) {
    if (size == 0) return false;
    bufferSize_ = size;
    return true;
  }
  uint16_t getBufferSize() const { return bufferSize_; }

  boolean connect(const char* id) { return connect(id, nullptr, nullptr); }
  boolean connect(const char* id, const char* user, const char* pass) {
    return connect(id, user, pass, nullptr, 0, false, nullptr);
  }
  boolean connect(const char* id, const char* willTopic, uint8_t willQos, boolean willRetain, const char* willMessage) {
    return connect(id, nullptr, nullptr, willTopic, willQos, willRetain, willMessage);
  }
  boolean connect(const char* id, const char* user, const char* pass, const char* willTopic, uint8_t willQos,
                  boolean willRetain, const char* willMessage, boolean cleanSession = true) {
    (void)willQos; (void)willRetain; (void)cleanSession;
    clientId_ = id ? id : "";
    user_ = user ? user : "";
    pass_ = pass ? pass : "";
    willTopic_ = willTopic ? willTopic : "";
    willMessage_ = willMessage ? willMessage : "";
    connectCalls_++;
    connected_ = connectResult_;
    state_ = connected_ ? MQTT_CONNECTED : MQTT_CONNECT_FAILED;
    if (connected_) subscriptions_.clear();  // a new session has no subscriptions
    return connected_;
  }
  void disconnect() { connected_ = false; state_ = MQTT_DISCONNECTED; subscriptions_.clear(); }

  boolean publish(const char* topic, const char* payload) { return publish(topic, payload, false); }
  boolean publish(const char* topic, const char* payload, boolean retained) {
    return publish(topic, reinterpret_cast<const uint8_t*>(payload), payload ? strlen(payload) : 0, retained);
  }
  boolean publish(const char* topic, const uint8_t* payload, unsigned int plength) {
    return publish(topic, payload, plength, false);
  }
  boolean publish(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained) {
    if (!connected_ || !topic) return false;
    // Same check as the real client: fixed header + topic length field + topic + payload
    if (bufferSize_ < MQTT_MAX_HEADER_SIZE + 2 + strlen(topic) + plength) return false;
    if (failPublishes_ > 0) { failPublishes_--; failedPublishes_++; return false; }
    published_.push_back({topic, std::string(reinterpret_cast<const char*>(payload), plength), retained != 0});
    return true;
  }
  boolean publish_P(const char* topic, const char* payload, boolean retained) { return publish(topic, payload, retained); }
  boolean publish_P(const char* topic, const uint8_t* payload, unsigned int plength, boolean retained) {
    return publish(topic, payload, plength, retained);
  }

  boolean beginPublish(const char* topic, unsigned int plength, boolean retained) {
    if (!connected_ || !topic) return false;
    streaming_ = true;
    streamTopic_ = topic;
    streamRetained_ = retained != 0;
    streamExpected_ = plength;
    streamPayload_.clear();
    return true;
  }
  int endPublish() {
    if (!streaming_) return 0;
    streaming_ = false;
    if (streamPayload_.size() != streamExpected_) return 0;
    published_.push_back({streamTopic_, streamPayload_, streamRetained_});
    return 1;
  }
  size_t write(uint8_t c) override {
    if (!streaming_) return 0;
    streamPayload_ += static_cast<char>(c);
    return 1;
  }
  using Print::write;

  boolean subscribe(const char* topic) { return subscribe(topic, 0); }
  boolean subscribe(const char* topic, uint8_t qos) {
    if (!connected_ || !topic || qos > 1) return false;
    if (bufferSize_ < 9 + strlen(topic)) return false;
    if (failSubscribe_) return false;
    for (auto& s : subscriptions_) if (s == topic) return true;
    subscriptions_.push_back(topic);
    return true;
  }
  boolean unsubscribe(const char* topic) {
    if (!connected_ || !topic) return false;
    for (auto it = subscriptions_.begin(); it != subscriptions_.end(); ++it) {
      if (*it == topic) { subscriptions_.erase(it); return true; }
    }
    return true;
  }

  // Delivers the messages queued with queueIncoming() to the callback, like
  // the real loop() does for packets received from the broker.
  boolean loop() {
    if (!connected_) return false;
    loopCalls_++;
    while (!incoming_.empty()) {
      Message m = incoming_.front();
      incoming_.pop_front();
      dispatch(m);
    }
    return true;
  }
  boolean connected() { return connected_; }
  int state() { return state_; }

  // ---- test helpers ----
  void reset() {
    connected_ = false;
    connectResult_ = true;
    state_ = MQTT_DISCONNECTED;
    bufferSize_ = MQTT_MAX_PACKET_SIZE;
    published_.clear();
    subscriptions_.clear();
    incoming_.clear();
    failPublishes_ = 0;
    failedPublishes_ = 0;
    failSubscribe_ = false;
    connectCalls_ = 0;
    loopCalls_ = 0;
    streaming_ = false;
  }
  // Force the connection state without going through connect()
  void setConnected(bool connected) { connected_ = connected; state_ = connected ? MQTT_CONNECTED : MQTT_CONNECTION_LOST; }
  // What the next connect() calls return
  void setConnectResult(bool ok) { connectResult_ = ok; }
  // Make the next n publish() calls fail (after the buffer size check)
  void failNextPublishes(int n) { failPublishes_ = n; }
  void failSubscribe(bool fail) { failSubscribe_ = fail; }
  // Hand a message to the callback immediately, as if it arrived from the broker
  void deliver(const std::string& topic, const std::string& payload) { dispatch({topic, payload, false}); }
  // Queue a message for the next loop()
  void queueIncoming(const std::string& topic, const std::string& payload) { incoming_.push_back({topic, payload, false}); }
  bool isSubscribed(const std::string& topic) const {
    for (auto& s : subscriptions_) if (s == topic) return true;
    return false;
  }

  std::vector<Message>& published() { return published_; }
  const std::vector<Message>& published() const { return published_; }
  const std::vector<std::string>& subscriptions() const { return subscriptions_; }
  int failedPublishes() const { return failedPublishes_; }
  int connectCalls() const { return connectCalls_; }
  int loopCalls() const { return loopCalls_; }
  const std::string& clientId() const { return clientId_; }

 private:
  void dispatch(const Message& m) {
    if (!callback_) return;
    // The real client hands out mutable buffers that are reused afterwards
    std::vector<char> topic(m.topic.begin(), m.topic.end());
    topic.push_back('\0');
    std::vector<uint8_t> payload(m.payload.begin(), m.payload.end());
    callback_(topic.data(), payload.data(), static_cast<unsigned int>(payload.size()));
  }

  Client* client_ = nullptr;
  std::string domain_;
  uint16_t port_ = 1883;
  std::function<void(char*, uint8_t*, unsigned int)> callback_;
  uint16_t keepAlive_ = MQTT_KEEPALIVE;
  uint16_t socketTimeout_ = MQTT_SOCKET_TIMEOUT;
  uint16_t bufferSize_ = MQTT_MAX_PACKET_SIZE;
  bool connected_ = false;
  bool connectResult_ = true;
  int state_ = MQTT_DISCONNECTED;
  std::string clientId_, user_, pass_, willTopic_, willMessage_;
  std::vector<Message> published_;
  std::vector<std::string> subscriptions_;
  std::deque<Message> incoming_;
  int failPublishes_ = 0;
  int failedPublishes_ = 0;
  bool failSubscribe_ = false;
  int connectCalls_ = 0;
  int loopCalls_ = 0;
  bool streaming_ = false;
  std::string streamTopic_, streamPayload_;
  bool streamRetained_ = false;
  size_t streamExpected_ = 0;
};

#endif
