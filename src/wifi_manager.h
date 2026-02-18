#pragma once
#include <Arduino.h>

namespace WiFiManager {
  void begin(const char* ssid, const char* pass);
  void ensureConnected();
  bool isConnected();
  String ipString();
}
