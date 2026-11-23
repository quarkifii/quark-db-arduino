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

// QuarkMqttTransport: the QuarkTransport implementation over PubSubClient,
// and the MQTT topic matching used for the subscribe topic.
#include <gtest/gtest.h>

#include <PubSubClient.h>
#include <QuarkMqttTransport.h>
#include <QuarkTransport.h>
#include <sync/sync_manager.h>

#include <string>

class CloudTransportTest : public ::testing::Test {
 protected:
  void SetUp() override { mqtt.reset(); }
  PubSubClient mqtt;
  QuarkMqttTransport transport{mqtt};
};

TEST_F(CloudTransportTest, ConnectedFollowsClient) {
  EXPECT_FALSE(transport.connected());
  mqtt.connect("esp-1");
  EXPECT_TRUE(transport.connected());
  mqtt.disconnect();
  EXPECT_FALSE(transport.connected());
}

TEST_F(CloudTransportTest, PublishGoesToClientNotRetainedByDefault) {
  EXPECT_FALSE(transport.publish("out", "{}"));  // offline
  mqtt.connect("esp-1");
  EXPECT_TRUE(transport.publish("out", "{\"a\":1}"));
  ASSERT_EQ(mqtt.published().size(), 1u);
  EXPECT_EQ(mqtt.published()[0].topic, "out");
  EXPECT_EQ(mqtt.published()[0].payload, "{\"a\":1}");
  EXPECT_FALSE(mqtt.published()[0].retained);
}

TEST_F(CloudTransportTest, RetainedOption) {
  QuarkMqttTransport retained(mqtt, true);
  mqtt.connect("esp-1");
  EXPECT_TRUE(retained.publish("out", "x"));
  EXPECT_TRUE(mqtt.published()[0].retained);
  EXPECT_EQ(&retained.getClient(), &mqtt);
}

TEST_F(CloudTransportTest, PublishFailsWhenClientRejects) {
  mqtt.connect("esp-1");
  mqtt.failNextPublishes(1);
  EXPECT_FALSE(transport.publish("out", "x"));
  std::string huge(MQTT_MAX_PACKET_SIZE, 'x');
  EXPECT_FALSE(transport.publish("out", huge.c_str()));
}

TEST_F(CloudTransportTest, SubscribeGoesToClient) {
  EXPECT_FALSE(transport.subscribe("in"));
  mqtt.connect("esp-1");
  EXPECT_TRUE(transport.subscribe("in"));
  EXPECT_TRUE(mqtt.isSubscribed("in"));
}

TEST_F(CloudTransportTest, MaxPayloadSizeTracksClientBuffer) {
  // buffer - fixed header - 2 byte topic length - topic
  EXPECT_EQ(transport.maxPayloadSize("abc"), (size_t)(MQTT_MAX_PACKET_SIZE - MQTT_MAX_HEADER_SIZE - 2 - 3));
  mqtt.setBufferSize(1024);
  EXPECT_EQ(transport.maxPayloadSize("abc"), 1024u - 10u);
  mqtt.setBufferSize(8);
  EXPECT_EQ(transport.maxPayloadSize("abc"), 0u);
  // a payload of exactly maxPayloadSize goes through the real buffer check
  mqtt.setBufferSize(64);
  mqtt.connect("esp-1");
  std::string fits(transport.maxPayloadSize("abc"), 'x');
  EXPECT_TRUE(transport.publish("abc", fits.c_str()));
  EXPECT_FALSE(transport.publish("abc", (fits + "x").c_str()));
}

TEST_F(CloudTransportTest, DefaultTransportHasNoPayloadLimit) {
  struct Plain : public QuarkTransport {
    bool connected() override { return true; }
    bool publish(const char*, const char*) override { return true; }
    bool subscribe(const char*) override { return true; }
  } plain;
  EXPECT_EQ(plain.maxPayloadSize("x"), 0u);
}

// ----------------------------------------------------- topic matching ----

TEST(CloudTopicMatch, ExactTopics) {
  EXPECT_TRUE(DBSyncManager::topicMatches("a/b/c", "a/b/c"));
  EXPECT_FALSE(DBSyncManager::topicMatches("a/b/c", "a/b"));
  EXPECT_FALSE(DBSyncManager::topicMatches("a/b", "a/b/c"));
  EXPECT_FALSE(DBSyncManager::topicMatches("a/b/c", "a/b/d"));
  EXPECT_FALSE(DBSyncManager::topicMatches("", "a"));
  EXPECT_TRUE(DBSyncManager::topicMatches("", ""));
  EXPECT_FALSE(DBSyncManager::topicMatches(nullptr, "a"));
  EXPECT_FALSE(DBSyncManager::topicMatches("a", nullptr));
}

TEST(CloudTopicMatch, SingleLevelWildcard) {
  EXPECT_TRUE(DBSyncManager::topicMatches("dev/+/in", "dev/esp-1/in"));
  EXPECT_FALSE(DBSyncManager::topicMatches("dev/+/in", "dev/esp-1/x/in"));
  EXPECT_FALSE(DBSyncManager::topicMatches("dev/+/in", "dev/esp-1"));
  EXPECT_TRUE(DBSyncManager::topicMatches("+", "anything"));
  EXPECT_TRUE(DBSyncManager::topicMatches("+/+", "a/b"));
  EXPECT_FALSE(DBSyncManager::topicMatches("+/+", "a/b/c"));
  EXPECT_TRUE(DBSyncManager::topicMatches("dev/+", "dev/"));
}

TEST(CloudTopicMatch, MultiLevelWildcard) {
  EXPECT_TRUE(DBSyncManager::topicMatches("dev/#", "dev/esp-1/in"));
  EXPECT_TRUE(DBSyncManager::topicMatches("dev/#", "dev/esp-1"));
  EXPECT_TRUE(DBSyncManager::topicMatches("dev/#", "dev"));  // MQTT: parent level matches too
  EXPECT_FALSE(DBSyncManager::topicMatches("dev/#", "device"));
  EXPECT_TRUE(DBSyncManager::topicMatches("#", "a/b/c"));
  EXPECT_TRUE(DBSyncManager::topicMatches("dev/+/#", "dev/esp-1/in/x"));
}
