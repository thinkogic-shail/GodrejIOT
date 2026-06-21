#include "mqtt_manager.h"
#include "machine_info.h"
#include "storage.h"

static PubSubClient* g_client = nullptr;
static const char* g_host = nullptr;
static uint16_t g_port = 1883;
static String g_uniqueCode;
static String g_user;
static String g_pass;

static String g_getInfoTopic;
static bool g_wildcard = false;
static uint32_t g_lastConnectAttemptAt = 0;
static int g_lastConnectState = INT32_MIN;

static constexpr uint32_t MQTT_RETRY_INTERVAL_MS = 5000;

static void onMessage(char* topic, byte* payload, unsigned int len) {
  String topicStr = topic;

  String msg;
  msg.reserve(len);
  for (unsigned int i = 0; i < len; i++) msg += (char)payload[i];

  Serial.println("\n========== MQTT CALLBACK ==========");
  Serial.print("Topic: "); Serial.println(topicStr);
  Serial.print("Payload: "); Serial.println(msg);
  Serial.print("Payload length: ");
  Serial.println(len);

  if (topicStr == g_getInfoTopic) {
    Serial.println("Matched getinfo for this device");
    if (MachineInfoStore::parseAndStore(msg)) {
      Serial.println("Machine info stored in memory");
      MachineInfoStore::printToSerial();

      const auto& info = MachineInfoStore::get();
      if (info.valid && info.MachineId > 0) {
        if (Storage::saveMachineInfo(info)) {
          Serial.println("MachineInfo saved to NVS");
        } else {
          Serial.println("MachineInfo save to NVS failed");
        }
      }
    } else {
      Serial.println("Failed to parse/store Machine INFO");
    }
  } else {
    Serial.println("Ignored (not getinfo for this device)");
  }

  Serial.println("==================================");
}

namespace MqttManager {

void begin(PubSubClient& client, const char* host, uint16_t port, const String& uniqueCode,
           const char* user, const char* pass) {
  g_client = &client;
  g_host = host;
  g_port = port;
  g_uniqueCode = uniqueCode;
  g_user = user ? user : "";
  g_pass = pass ? pass : "";

  g_client->setServer(g_host, g_port);
  g_client->setCallback(onMessage);
  g_client->setKeepAlive(30);
  g_client->setSocketTimeout(3);
  g_client->setBufferSize(2048);

  g_lastConnectAttemptAt = 0;
  g_lastConnectState = INT32_MIN;
}

bool isConnected() {
  return g_client && g_client->connected();
}

void ensureConnected() {
  if (!g_client || g_client->connected()) return;

  uint32_t now = millis();
  if (g_lastConnectAttemptAt != 0 && (now - g_lastConnectAttemptAt) < MQTT_RETRY_INTERVAL_MS) {
    return;
  }

  String clientId = "GodrejIOT-" + g_uniqueCode;
  Serial.println("Connecting MQTT...");
  Serial.println(clientId);

  g_lastConnectAttemptAt = now;
  bool connected = false;
  if (g_user.length() > 0) {
    connected = g_client->connect(clientId.c_str(), g_user.c_str(), g_pass.c_str());
  } else {
    connected = g_client->connect(clientId.c_str());
  }

  if (!connected) {
    int state = g_client->state();
    if (state != g_lastConnectState) {
      Serial.print("MQTT connect failed rc=");
      Serial.println(state);
      g_lastConnectState = state;
    }
    return;
  }

  g_lastConnectState = 0;
  Serial.println("MQTT connected");

  if (g_wildcard) {
    g_client->subscribe("godrej/#", 1);
    Serial.println("Subscribed: godrej/# (QoS1)");
  }
  if (g_getInfoTopic.length() > 0) {
    g_client->subscribe(g_getInfoTopic.c_str(), 1);
    Serial.print("Subscribed: "); Serial.println(g_getInfoTopic);
  }
}

void subscribeTopics(const String& getInfoTopic, bool wildcard, uint8_t qos) {
  (void)qos;
  g_getInfoTopic = getInfoTopic;
  g_wildcard = wildcard;

  if (!g_client || !g_client->connected()) return;

  if (g_wildcard) {
    g_client->subscribe("godrej/#", 1);
    Serial.println("Subscribed: godrej/# (QoS1)");
  }
  g_client->subscribe(g_getInfoTopic.c_str(), 1);
  Serial.print("Subscribed: "); Serial.println(g_getInfoTopic);
}

bool publishJson(const String& topic, const String& payload) {
  if (!g_client || !g_client->connected()) return false;
  return g_client->publish(topic.c_str(), payload.c_str(), false);
}

void loop() {
  if (g_client && g_client->connected()) g_client->loop();
}

} // namespace
