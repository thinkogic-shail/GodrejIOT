#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

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


static String buildInfoRequestJson() {
  String payload = "{";
  payload += "\"MachineId\":0,";
  payload += "\"UniqueCode\":\"" + String(UNIQUE_CODE) + "\",";
  payload += "\"RequestType\":\"Info\"";
  payload += "}";
  return payload;
}

static String buildBootHeartbeatStatusJson() {
  int machineId = 0;

  // If machine info already stored (provisioned), include it
  MachineInfo cached;
  if (Storage::loadMachineInfo(cached)) {
    machineId = cached.MachineId;
  }

  String request_type = "Status";

  // NOTE: You don't have RTC/NTP yet. Keeping millis-based timestamp for now.
  // Later you can replace with real datetime string.
  //String statusDateTime = String(millis() / 1000);
  String statusDateTime = "18/02/2026, 15:15:24	";

  String statusData = "{";
  statusData += "\"MachineId\":" + String(machineId);
  statusData += ",\"UniqueCode\":\"" + String(UNIQUE_CODE) + "\"";
  statusData += ",\"ErrorLevel\":\"0\"";
  statusData += ",\"ErrorNo\":\"0\"";
  statusData += ",\"ErrorName\":\"BOOT_HEARTBEAT\"";
  statusData += ",\"StatusDateTime\":\"" + statusDateTime + "\"";
  statusData += ",\"RequestType\":\"" + request_type + "\"";
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
  // TEMP: uncomment only when you want factory reset
 //Storage::factoryReset(true);

  Serial.begin(115200);
  delay(200);

  Storage::begin(); //NVS Initialization

  ButtonManager::begin(RESET_BUTTON_PIN, RESET_BUTTON_ACTIVE_LOW);

  // BLEManager::begin(String("THINK#") + UNIQUE_CODE);
  Serial.println("Starting BLE...");
  String bleName = String("THINK-") + UNIQUE_CODE;
  // if (!Storage::isProvisioned()) {
  //   bleName += "-NEW";
  // }
  BLEManager::begin(bleName);
  Serial.println("BLE begin called ✅");

  


  // Optional: load cached machine info for logging/verification
  MachineInfo cached;
  if (Storage::loadMachineInfo(cached)) {
    Serial.println("✅ Loaded MachineInfo from NVS (flash). Provisioning not required.");
    Serial.print("Cached MachineId: ");
    Serial.println(cached.MachineId);
    Serial.print("Cached MachineName: ");
    Serial.println(cached.MachineName);
  } else {
    Serial.println("ℹ️ No MachineInfo in NVS. Provisioning will run.");
  }

  Serial.println("=================================");
  Serial.println("GodrejIOT – Modular Firmware");
  Serial.print("UniqueCode: "); Serial.println(UNIQUE_CODE);
  Serial.print("GetInfo: "); Serial.println(topicGetInfo());
  Serial.print("SendInfo: "); Serial.println(topicSendInfo());
  Serial.print("SendStatus: "); Serial.println(topicSendStatus());  // ✅ HERE
  Serial.println("=================================");

  // WiFiManager::begin(WIFI_SSID, WIFI_PASS);
  String ssid, pass;

  if (Storage::loadWiFi(ssid, pass)) {
    Serial.println("✅ Using WiFi from NVS (set via BLE)");
    WiFiManager::begin(ssid.c_str(), pass.c_str());
  } else {
    Serial.println("ℹ️ No WiFi in NVS, using config.h defaults");
    WiFiManager::begin(WIFI_SSID, WIFI_PASS);
  }

  MqttManager::begin(mqtt, MQTT_HOST, MQTT_PORT, String(UNIQUE_CODE));
}

void loop() {
  WiFiManager::ensureConnected();

  if (WiFiManager::isConnected()) {
    MqttManager::ensureConnected();

    if (MqttManager::isConnected()) {

      // ✅ Subscribe only once per connect
      if (!topicsSubscribed) {
        MqttManager::subscribeTopics(topicGetInfo(), false, 1); // wildcard=false for production
        topicsSubscribed = true;

        // Reset one-shot flags on a fresh connect
        infoRequestSent = false;
        bootHeartbeatSent = false; // ✅ NEW
      }

      // ✅ Always keep MQTT alive
      MqttManager::loop();

      // ✅ Boot heartbeat (ONE time per MQTT connect)
      if (!bootHeartbeatSent) {
          String hb = buildBootHeartbeatStatusJson();
          bool ok = MqttManager::publishJson(topicSendStatus(), hb);

          Serial.print("Published Boot Heartbeat → ");
          Serial.print(topicSendStatus());
          Serial.print(" ok=");
          Serial.println(ok ? "1" : "0");
          Serial.println(hb);

          bootHeartbeatSent = true;
      }


      //  if (!infoRequestSent) {
      //     String payload = buildInfoRequestJson();
      //     bool ok = MqttManager::publishJson(topicSendInfo(), payload);

      //     Serial.print("Published Info Request → ");
      //     Serial.print(topicSendInfo());
      //     Serial.print(" ok=");
      //     Serial.println(ok ? "1" : "0");
      //     Serial.println(payload);

      //     infoRequestSent = true;
      //   }
      // ✅ If already provisioned, DO NOT send Info request again
      if (Storage::isProvisioned()) {
        // Provisioning is complete; move to next phase later:
        // - publish heartbeat/status
        // - UART reading
        // - BLE config updates
        static bool printedOnce = false;
        if (!printedOnce) {
          Serial.println("✅ Device is provisioned (NVS). Skipping Info request.");
          printedOnce = true;
        }

      } else {
        // ❗ Not provisioned → send Info request (only once for now)
        if (!infoRequestSent) {
          String payload = buildInfoRequestJson();
          bool ok = MqttManager::publishJson(topicSendInfo(), payload);

          Serial.print("Published Info Request → ");
          Serial.print(topicSendInfo());
          Serial.print(" ok=");
          Serial.println(ok ? "1" : "0");
          Serial.println(payload);

          infoRequestSent = true;
        }
      }

    } else {
      // If MQTT disconnected, allow re-subscribe after reconnect
      topicsSubscribed = false;
    }
  } else {
    // If WiFi disconnected, allow re-init after reconnect
    topicsSubscribed = false;
  }

  ButtonManager::loop();
  BLEManager::loop();

  delay(10);


}
