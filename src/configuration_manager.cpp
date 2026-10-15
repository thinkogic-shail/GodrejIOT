#include "configuration_manager.h"
#include "storage.h"
#include "mqtt_manager.h"
#include "telemetry_settings.h"
#include <ArduinoJson.h>

namespace {
ConfigurationManager::Configuration g_configuration;
bool g_requested = false;

bool validTime(const String& value) {
  return value.length() == 5 && value[2] == ':' &&
      value[0] >= '0' && value[0] <= '2' &&
      value[1] >= '0' && value[1] <= '9' &&
      (value[0] != '2' || value[1] <= '3') &&
      value[3] >= '0' && value[3] <= '5' &&
      value[4] >= '0' && value[4] <= '9';
}

bool decode(const String& json, ConfigurationManager::Configuration& output) {
  JsonDocument doc;
  if (deserializeJson(doc, json) || !doc.is<JsonObject>()) return false;
  if (!doc["ApkId"].is<uint32_t>()) return false;
  for (const char* key : {"ApkVersion", "FilePath", "IsActive",
                          "VideoDownloadStartTime", "VideoDownloadStopTime"}) {
    if (!doc[key].is<const char*>()) return false;
  }
  for (const char* key : {"MQTTPushInterval", "MQTTStatisticsInterval",
                          "ScreensaverInactiveTimeout", "MQTTPushCounterInterval"}) {
    // Positive integer milliseconds; bound to a safe millis() scheduling range.
    if (!doc[key].is<uint32_t>() || doc[key].as<uint32_t>() == 0 ||
        doc[key].as<uint32_t>() > INT32_MAX) return false;
  }
  output.ApkId = doc["ApkId"].as<uint32_t>();
  output.ApkVersion = doc["ApkVersion"].as<String>();
  output.FilePath = doc["FilePath"].as<String>();
  output.IsActive = doc["IsActive"].as<String>();
  output.VideoDownloadStartTime = doc["VideoDownloadStartTime"].as<String>();
  output.VideoDownloadStopTime = doc["VideoDownloadStopTime"].as<String>();
  if (!validTime(output.VideoDownloadStartTime) || !validTime(output.VideoDownloadStopTime))
    return false;
  output.MQTTPushInterval = doc["MQTTPushInterval"].as<uint32_t>();
  output.MQTTStatisticsInterval = doc["MQTTStatisticsInterval"].as<uint32_t>();
  output.ScreensaverInactiveTimeout = doc["ScreensaverInactiveTimeout"].as<uint32_t>();
  output.MQTTPushCounterInterval = doc["MQTTPushCounterInterval"].as<uint32_t>();
  return true;
}

String encode(const ConfigurationManager::Configuration& value) {
  JsonDocument doc;
  doc["ApkId"] = value.ApkId;
  doc["ApkVersion"] = value.ApkVersion;
  doc["FilePath"] = value.FilePath;
  doc["IsActive"] = value.IsActive;
  doc["VideoDownloadStartTime"] = value.VideoDownloadStartTime;
  doc["VideoDownloadStopTime"] = value.VideoDownloadStopTime;
  doc["MQTTPushInterval"] = value.MQTTPushInterval;
  doc["MQTTStatisticsInterval"] = value.MQTTStatisticsInterval;
  doc["ScreensaverInactiveTimeout"] = value.ScreensaverInactiveTimeout;
  doc["MQTTPushCounterInterval"] = value.MQTTPushCounterInterval;
  String json;
  serializeJson(doc, json);
  return json;
}

void apply(const ConfigurationManager::Configuration& value) {
  g_configuration = value;
  TelemetrySettings::statusIntervalMs.store(value.MQTTPushInterval);
  TelemetrySettings::counterIntervalMs.store(value.MQTTPushCounterInterval);
}
}

namespace ConfigurationManager {
void reset() {
  apply(Configuration{});
  g_requested = false;
}

void begin() {
  reset();
  String json;
  Configuration saved;
  if (Storage::loadConfiguration(json) && decode(json, saved)) apply(saved);
}

const Configuration& get() { return g_configuration; }

bool accept(const String& json) {
  Configuration next;
  if (!decode(json, next) || !Storage::saveConfiguration(encode(next))) return false;
  apply(next);
  return true;
}

void requestIfNeeded(const String& uniqueCode) {
  if (g_requested || uniqueCode.isEmpty() || !MqttManager::isConnected()) return;
  MachineInfo info;
  if (!Storage::loadMachineInfo(info)) return;
  // One attempt per boot, including publish failure or a later reconnect.
  // Normal publishing already uses restored settings; no response is required.
  g_requested = true;
  JsonDocument doc;
  doc["MachineId"] = info.MachineId;
  doc["UniqueCode"] = uniqueCode;
  doc["RequestType"] = "Configuration";
  String payload;
  serializeJson(doc, payload);
  MqttManager::publishJson("godrej/configuration/" + uniqueCode, payload);
}
}
