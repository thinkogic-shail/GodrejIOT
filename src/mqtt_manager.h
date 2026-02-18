#pragma once
#include <Arduino.h>
#include <PubSubClient.h>

namespace MqttManager {
  void begin(PubSubClient& client, const char* host, uint16_t port, const String& uniqueCode);
  void ensureConnected();

  // Subscribe / publish helpers
  void subscribeTopics(const String& getInfoTopic, bool wildcard = false, uint8_t qos = 1);
  bool publishJson(const String& topic, const String& payload);

  // Must be called in loop
  void loop();

  bool isConnected();
}
