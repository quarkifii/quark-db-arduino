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

// Sanity checks for the PubSubClient mock: it has to behave like the real
// client where QuarkDB depends on it (connection state, publish rules,
// subscriptions, callback delivery).
#include <gtest/gtest.h>

#include <PubSubClient.h>

#include <string>

class CloudMqttMockTest : public ::testing::Test {
 protected:
  void SetUp() override { mqtt.reset(); }
  WiFiClient wifi;
  PubSubClient mqtt{wifi};
};

TEST_F(CloudMqttMockTest, StartsDisconnected) {
  EXPECT_FALSE(mqtt.connected());
  EXPECT_EQ(mqtt.state(), MQTT_DISCONNECTED);
  EXPECT_EQ(mqtt.getBufferSize(), MQTT_MAX_PACKET_SIZE);
}

TEST_F(CloudMqttMockTest, ConnectAndDisconnect) {
  EXPECT_TRUE(mqtt.connect("esp-1"));
  EXPECT_TRUE(mqtt.connected());
  EXPECT_EQ(mqtt.state(), MQTT_CONNECTED);
  EXPECT_EQ(mqtt.clientId(), "esp-1");
  mqtt.disconnect();
  EXPECT_FALSE(mqtt.connected());
  mqtt.setConnectResult(false);
  EXPECT_FALSE(mqtt.connect("esp-1"));
  EXPECT_EQ(mqtt.state(), MQTT_CONNECT_FAILED);
  EXPECT_EQ(mqtt.connectCalls(), 2);
}

TEST_F(CloudMqttMockTest, PublishRequiresConnection) {
  EXPECT_FALSE(mqtt.publish("t", "x"));
  EXPECT_TRUE(mqtt.published().empty());
  mqtt.connect("esp-1");
  EXPECT_TRUE(mqtt.publish("t", "x"));
  ASSERT_EQ(mqtt.published().size(), 1u);
  EXPECT_EQ(mqtt.published()[0].topic, "t");
  EXPECT_EQ(mqtt.published()[0].payload, "x");
  EXPECT_FALSE(mqtt.published()[0].retained);
  EXPECT_TRUE(mqtt.publish("t", "y", true));
  EXPECT_TRUE(mqtt.published()[1].retained);
}

TEST_F(CloudMqttMockTest, PublishEnforcesBufferSizeLikeRealClient) {
  mqtt.connect("esp-1");
  // real rule: MQTT_MAX_HEADER_SIZE + 2 + strlen(topic) + plength <= bufferSize
  std::string topic = "abc";
  std::string fits(MQTT_MAX_PACKET_SIZE - MQTT_MAX_HEADER_SIZE - 2 - topic.size(), 'x');
  EXPECT_TRUE(mqtt.publish(topic.c_str(), fits.c_str()));
  std::string tooBig = fits + "x";
  EXPECT_FALSE(mqtt.publish(topic.c_str(), tooBig.c_str()));
  ASSERT_TRUE(mqtt.setBufferSize(1024));
  EXPECT_TRUE(mqtt.publish(topic.c_str(), tooBig.c_str()));
  EXPECT_FALSE(mqtt.setBufferSize(0));
}

TEST_F(CloudMqttMockTest, BinaryPublishKeepsLength) {
  mqtt.connect("esp-1");
  const uint8_t raw[] = {'a', 0, 'b'};
  EXPECT_TRUE(mqtt.publish("t", raw, 3));
  EXPECT_EQ(mqtt.published()[0].payload, std::string("a\0b", 3));
}

TEST_F(CloudMqttMockTest, StreamedPublish) {
  mqtt.connect("esp-1");
  ASSERT_TRUE(mqtt.beginPublish("t", 5, false));
  mqtt.print("hello");
  EXPECT_EQ(mqtt.endPublish(), 1);
  EXPECT_EQ(mqtt.published()[0].payload, "hello");
  ASSERT_TRUE(mqtt.beginPublish("t", 5, false));
  mqtt.print("hi");
  EXPECT_EQ(mqtt.endPublish(), 0);  // length mismatch, like the real client
}

TEST_F(CloudMqttMockTest, FailNextPublishes) {
  mqtt.connect("esp-1");
  mqtt.failNextPublishes(2);
  EXPECT_FALSE(mqtt.publish("t", "1"));
  EXPECT_FALSE(mqtt.publish("t", "2"));
  EXPECT_TRUE(mqtt.publish("t", "3"));
  EXPECT_EQ(mqtt.failedPublishes(), 2);
  EXPECT_EQ(mqtt.published().size(), 1u);
}

TEST_F(CloudMqttMockTest, SubscriptionsNeedConnectionAndAreLostOnReconnect) {
  EXPECT_FALSE(mqtt.subscribe("in"));
  mqtt.connect("esp-1");
  EXPECT_TRUE(mqtt.subscribe("in"));
  EXPECT_TRUE(mqtt.subscribe("in"));  // idempotent
  EXPECT_TRUE(mqtt.isSubscribed("in"));
  EXPECT_EQ(mqtt.subscriptions().size(), 1u);
  EXPECT_FALSE(mqtt.subscribe("in", 2));  // qos 2 unsupported
  EXPECT_TRUE(mqtt.unsubscribe("in"));
  EXPECT_FALSE(mqtt.isSubscribed("in"));
  EXPECT_TRUE(mqtt.subscribe("in"));
  mqtt.disconnect();
  mqtt.connect("esp-1");
  EXPECT_FALSE(mqtt.isSubscribed("in"));
  mqtt.failSubscribe(true);
  EXPECT_FALSE(mqtt.subscribe("in"));
}

TEST_F(CloudMqttMockTest, DeliverInvokesCallbackWithTopicAndPayload) {
  std::string gotTopic, gotPayload;
  mqtt.setCallback([&](char* topic, uint8_t* payload, unsigned int length) {
    gotTopic = topic;
    gotPayload.assign(reinterpret_cast<char*>(payload), length);
  });
  mqtt.deliver("in", "{\"a\":1}");
  EXPECT_EQ(gotTopic, "in");
  EXPECT_EQ(gotPayload, "{\"a\":1}");
}

TEST_F(CloudMqttMockTest, QueuedMessagesAreDeliveredByLoop) {
  int calls = 0;
  mqtt.setCallback([&](char*, uint8_t*, unsigned int) { calls++; });
  mqtt.queueIncoming("in", "1");
  mqtt.queueIncoming("in", "2");
  EXPECT_FALSE(mqtt.loop());  // not connected
  EXPECT_EQ(calls, 0);
  mqtt.connect("esp-1");
  EXPECT_TRUE(mqtt.loop());
  EXPECT_EQ(calls, 2);
  EXPECT_TRUE(mqtt.loop());
  EXPECT_EQ(calls, 2);
  EXPECT_EQ(mqtt.loopCalls(), 2);
}

TEST_F(CloudMqttMockTest, NoCallbackIsHarmless) {
  mqtt.deliver("in", "x");
  SUCCEED();
}

TEST_F(CloudMqttMockTest, ResetClearsEverything) {
  mqtt.connect("esp-1");
  mqtt.setBufferSize(900);
  mqtt.subscribe("in");
  mqtt.publish("t", "x");
  mqtt.reset();
  EXPECT_FALSE(mqtt.connected());
  EXPECT_EQ(mqtt.getBufferSize(), MQTT_MAX_PACKET_SIZE);
  EXPECT_TRUE(mqtt.published().empty());
  EXPECT_TRUE(mqtt.subscriptions().empty());
}
