#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>

#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "storage.h"
#include "machine_info.h"
#include "ble_manager.h"
#include "button_manager.h"

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

static bool topicsSubscribed = false;
static bool infoRequestSent = false;
static bool bootHeartbeatSent = false;
static String g_networkMode = DEFAULT_NETWORK_MODE;
static String g_mqttHost = MQTT_HOST;
static uint16_t g_mqttPort = MQTT_PORT;
static String g_mqttUser = MQTT_USER;
static String g_mqttPass = MQTT_PASS;

static String getCurrentUniqueCode() {
  String uniqueCode;
  if (Storage::loadUniqueCode(uniqueCode) && uniqueCode.length() > 0) {
    return uniqueCode;
  }
  return String(UNIQUE_CODE);
}

static String topicGetInfo(const String& uniqueCode) {
  return "godrej/getinfo/" + uniqueCode;
}

static String topicSendInfo(const String& uniqueCode) {
  return "godrej/sendinfo/" + uniqueCode;
}

static String topicSendStatus(const String& uniqueCode) {
  return "godrej/sendstatus/" + uniqueCode;
}

static String getChipIdHex() {
  uint64_t chipId = ESP.getEfuseMac();
  char buffer[13];
  snprintf(buffer, sizeof(buffer), "%012llX", chipId);
  return String(buffer);
}

static String buildBleName() {
  return String("THINK-") + getChipIdHex();
}

static bool isWifiMode() {
  return !g_networkMode.equalsIgnoreCase("SIM");
}

static String buildInfoRequestJson() {
  String uniqueCode = getCurrentUniqueCode();
  String payload = "{";
  payload += "\"MachineId\":0,";
  payload += "\"UniqueCode\":\"" + uniqueCode + "\",";
  payload += "\"RequestType\":\"Info\"";
  payload += "}";
  return payload;
}

static bool tryGetDeviceDateTime(String& outDateTime) {
  struct tm timeInfo;
  if (!getLocalTime(&timeInfo, 10)) {
    return false;
  }

  char buffer[24];
  strftime(buffer, sizeof(buffer), "%d/%m/%Y, %H:%M:%S", &timeInfo);
  outDateTime = buffer;
  return true;
}

static String buildStatusDateTime() {
  String statusDateTime;
  if (tryGetDeviceDateTime(statusDateTime)) {
    return statusDateTime;
  }

  // TODO: Enable once you finalize network time bootstrap for production.
  // Example:
  // configTime(19800, 0, "pool.ntp.org", "time.nist.gov");
  return "18/03/2026, 11:05:24";
}

static String buildBootHeartbeatStatusJson() {
  int machineId = 0;
  String uniqueCode = getCurrentUniqueCode();

  MachineInfo cached;
  if (Storage::loadMachineInfo(cached)) {
    machineId = cached.MachineId;
  }

  String statusData = "{";
  statusData += "\"MachineId\":" + String(machineId);
  statusData += ",\"UniqueCode\":\"" + uniqueCode + "\"";
  statusData += ",\"ErrorLevel\":\"0\"";
  statusData += ",\"ErrorNo\":\"0\"";
  statusData += ",\"ErrorName\":\"BOOT_HEARTBEAT\"";
  statusData += ",\"StatusDateTime\":\"" + buildStatusDateTime() + "\"";
  statusData += ",\"RequestType\":\"Status\"";
  statusData += ",\"ButtonNo\":\"0\"";
  statusData += ",\"EmployeeCode\":\"\"";
  statusData += ",\"EmployeeName\":\"\"";
  statusData += ",\"CardNo\":\"\"";
  statusData += ",\"FacilityCode\":\"\"";
  statusData += ",\"IssueType\":\"\"";
  statusData += "}";

  return statusData;
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("================================");
  Serial.println("SETUP STARTED");
  Serial.println("================================");

  if (!Storage::begin()) {
    Serial.println("Storage initialization failed");
  }

  // TEMP: Uncomment for one boot when you want to clear NVS and continue startup.
  // IMPORTANT: Comment it again after use, otherwise every boot will erase NVS.
  //Storage::factoryReset(false);

  ButtonManager::begin(RESET_BUTTON_PIN, RESET_BUTTON_ACTIVE_LOW);

  Serial.println("Starting BLE...");
  String bleName = buildBleName();
  Serial.print("BLE Name: ");
  Serial.println(bleName);
  BLEManager::begin(bleName);
  Serial.println("BLE begin called");

  MachineInfo cached;
  if (Storage::loadMachineInfo(cached)) {
    Serial.println("Loaded MachineInfo from NVS. Provisioning not required.");
    Serial.print("Cached MachineId: ");
    Serial.println(cached.MachineId);
    Serial.print("Cached MachineName: ");
    Serial.println(cached.MachineName);
  } else {
    Serial.println("No MachineInfo in NVS. Provisioning will run.");
  }

  Serial.println("=================================");
  Serial.println("GodrejIOT - Modular Firmware");
  String uniqueCode = getCurrentUniqueCode();
  Serial.print("UniqueCode: "); Serial.println(uniqueCode);
  Serial.print("GetInfo: "); Serial.println(topicGetInfo(uniqueCode));
  Serial.print("SendInfo: "); Serial.println(topicSendInfo(uniqueCode));
  Serial.print("SendStatus: "); Serial.println(topicSendStatus(uniqueCode));
  Serial.println("=================================");

  if (!Storage::loadNetworkMode(g_networkMode) || g_networkMode.length() == 0) {
    g_networkMode = DEFAULT_NETWORK_MODE;
  }
  Serial.print("NetworkMode: ");
  Serial.println(g_networkMode);

  if (!Storage::loadMqttSettings(g_mqttHost, g_mqttPort, g_mqttUser, g_mqttPass)) {
    g_mqttHost = MQTT_HOST;
    g_mqttPort = MQTT_PORT;
    g_mqttUser = MQTT_USER;
    g_mqttPass = MQTT_PASS;
  }
  Serial.print("MQTT Host: ");
  Serial.println(g_mqttHost);
  Serial.print("MQTT Port: ");
  Serial.println(g_mqttPort);

  String ssid;
  String pass;
  if (isWifiMode()) {
    if (Storage::loadWiFi(ssid, pass)) {
      Serial.println("Using WiFi from NVS (set via BLE)");
      WiFiManager::begin(ssid.c_str(), pass.c_str());
    } else {
      Serial.println("No WiFi in NVS, using config.h defaults");
      WiFiManager::begin(WIFI_SSID, WIFI_PASS);
    }
  } else {
    Serial.println("SIM mode selected. WiFi/MQTT over SIM is not implemented yet.");
  }

  MqttManager::begin(mqtt, g_mqttHost.c_str(), g_mqttPort, getCurrentUniqueCode(),
                     g_mqttUser.c_str(), g_mqttPass.c_str());
}

void loop() {
  if (!isWifiMode()) {
    ButtonManager::loop();
    BLEManager::loop();
    delay(10);
    return;
  }

  WiFiManager::ensureConnected();

  if (WiFiManager::isConnected()) {
    MqttManager::ensureConnected();

    if (MqttManager::isConnected()) {
      String uniqueCode = getCurrentUniqueCode();
      if (!topicsSubscribed) {
        MqttManager::subscribeTopics(topicGetInfo(uniqueCode), false, 1);
        topicsSubscribed = true;
        infoRequestSent = false;
        bootHeartbeatSent = false;
      }

      MqttManager::loop();

      if (!bootHeartbeatSent) {
        String hb = buildBootHeartbeatStatusJson();
        bool ok = MqttManager::publishJson(topicSendStatus(uniqueCode), hb);

        Serial.print("Published Boot Heartbeat -> ");
        Serial.print(topicSendStatus(uniqueCode));
        Serial.print(" ok=");
        Serial.println(ok ? "1" : "0");
        Serial.println(hb);

        bootHeartbeatSent = true;
      }

      if (Storage::isProvisioned()) {
        static bool printedOnce = false;
        if (!printedOnce) {
          Serial.println("Device is provisioned (NVS). Skipping Info request.");
          printedOnce = true;
        }
      } else if (!infoRequestSent) {
        String payload = buildInfoRequestJson();
        bool ok = MqttManager::publishJson(topicSendInfo(uniqueCode), payload);

        Serial.print("Published Info Request -> ");
        Serial.print(topicSendInfo(uniqueCode));
        Serial.print(" ok=");
        Serial.println(ok ? "1" : "0");
        Serial.println(payload);

        infoRequestSent = true;
      }
    } else {
      topicsSubscribed = false;
    }
  } else {
    topicsSubscribed = false;
  }

  ButtonManager::loop();
  BLEManager::loop();

  delay(10);
}
