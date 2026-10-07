#pragma once
#include <Arduino.h>

namespace BLEManager {
  void begin(const String& deviceName);
  void loop(); // optional (can be empty), call in main loop
  bool isConnected();
  bool isMaintenanceMode();
  void notifyText(const String& msg);
  void release();
}
