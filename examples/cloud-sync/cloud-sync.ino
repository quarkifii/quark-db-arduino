// Cloud backup of a QuarkDB list over MQTT.
//
// Every change to the "config" list is logged to its oplog and pushed to the
// broker as soon as the connection is up. While offline the changes keep
// accumulating on flash (up to maxOplogBytes), and on the next connection
// the device asks the cloud what it already has and sends only the missing
// part, or a full snapshot when the oplog rolled over.
//
// Needs the PubSubClient library (https://github.com/knolleary/pubsubclient).
// The cloud side (a service subscribed to the "out" topic that stores the
// snapshot/ops and answers queries on the "in" topic) is not part of this
// library; see API.md for the message formats.
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif
#include <PubSubClient.h>
#include <QuarkDB.h>
#include <QuarkMqttTransport.h>

const char* WIFI_SSID = "your-ssid";
const char* WIFI_PASS = "your-password";
const char* MQTT_HOST = "broker.example.com";
const int MQTT_PORT = 1883;
const char* DEVICE_ID = "esp-device-1";

QuarkDB quarkDB;
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
QuarkMqttTransport transport(mqtt);
QuarkSyncConfig syncConfig;

unsigned long lastReconnectAttempt = 0;
unsigned long lastWrite = 0;
int counter = 0;

// Every message from the broker goes to the DB; it ignores what is not a
// sync message and returns false, so other topics can be handled after it.
void onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
  if (quarkDB.handleMessage(topic, payload, length)) {
    return;
  }
  Serial.printf("Other message on %s\n", topic);
}

bool connectMqtt() {
  if (!mqtt.connect(DEVICE_ID)) {
    return false;
  }
  Serial.println("MQTT connected");
  // Subscribes to the "in" topic and asks the cloud what it holds for every
  // synced list. Must be called on every (re)connection.
  int lists = quarkDB.syncCloud();
  Serial.printf("Sync started for %d list(s)\n", lists);
  return true;
}

void setup() {
  Serial.begin(115200);
  while (!quarkDB.init(QUARKDB_SPIFFS_FILE_TYPE)) {
    delay(100);
  }
  // A list must exist before sync is enabled for it; sync settings persist
  // on flash so this is a no-op after the first boot.
  if (quarkDB.getRecordCount("config") < 0) {
    quarkDB.createList("config");
  }

  syncConfig.publishTopic = String("devices/") + DEVICE_ID + "/quarkdb/out";   // device -> cloud
  syncConfig.subscribeTopic = String("devices/") + DEVICE_ID + "/quarkdb/in";  // cloud -> device
  syncConfig.deviceId = DEVICE_ID;           // optional, added to every message
  syncConfig.maxOplogBytes = 4096;           // offline changes kept per list
  syncConfig.maxPayloadBytes = 1024;         // largest MQTT payload; snapshots are chunked to it
  syncConfig.restoreOnMismatch = false;      // device is master by default

  // PubSubClient's default buffer is 256 bytes: make room for the payload,
  // the topic and the MQTT header
  mqtt.setBufferSize(syncConfig.maxPayloadBytes + syncConfig.publishTopic.length() + 16);
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);

  quarkDB.enableSync(&transport, syncConfig);
  // This list may be rebuilt from the cloud copy if the flash was wiped or
  // the list file was lost (the device remembers the instances it deleted
  // on purpose, those are never restored)
  QuarkListSyncOptions options;
  options.restore = QUARK_RESTORE_ON_MISMATCH;
  quarkDB.enableListSync("config", options);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqtt.connected()) {
      if (millis() - lastReconnectAttempt > 5000) {
        lastReconnectAttempt = millis();
        connectMqtt();
      }
    } else {
      mqtt.loop();  // delivers cloud answers to onMqttMessage()
    }
  }

  // Writes work exactly as before, connected or not; the sync layer logs
  // them and publishes when it can
  if (millis() - lastWrite > 10000) {
    lastWrite = millis();
    JsonDocument doc;
    doc["key"] = "reading";
    doc["value"] = counter++;
    if (counter == 1) {
      quarkDB.addRecord("config", doc.as<JsonObject>());
    } else {
      quarkDB.updateRecords("config", "{\"key\":\"reading\"}", doc.as<JsonObject>());
    }

    JsonDocument info;
    if (quarkDB.getSyncInfo("config", &info)) {
      Serial.printf("config: seq=%ld cloudSeq=%ld pending=%d state=%s\n",
                    info["seq"].as<long>(), info["cloudSeq"].as<long>(),
                    info["pending"].as<int>(), info["state"].as<const char*>());
    }
  }

  // On demand pull of the cloud copy: type "restore" in the serial monitor
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "restore") {
      Serial.printf("restoreFromCloud -> %d\n", quarkDB.restoreFromCloud("config"));
    }
  }
}
