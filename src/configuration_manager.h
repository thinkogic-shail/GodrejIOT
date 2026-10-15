#pragma once
#include <Arduino.h>
#include <stdint.h>

namespace ConfigurationManager {
struct Configuration {
  uint32_t ApkId = 0;
  String ApkVersion;
  String FilePath;
  String IsActive;
  String VideoDownloadStartTime;
  String VideoDownloadStopTime;
  uint32_t MQTTPushInterval = 5000;
  uint32_t MQTTStatisticsInterval = 3600000;
  uint32_t ScreensaverInactiveTimeout = 10000;
  uint32_t MQTTPushCounterInterval = 10000;
};

// Called after Storage::begin; restores settings and allows one request this boot.
void begin();
void reset();
const Configuration& get();
// Rejects malformed configuration without changing saved or active settings.
bool accept(const String& json);
// Main loop, after MQTT subscriptions and machine provisioning; one attempt per boot.
void requestIfNeeded(const String& uniqueCode);
}
