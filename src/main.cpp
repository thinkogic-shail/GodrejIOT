#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <ArduinoJson.h>

#include "config.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "storage.h"
#include "machine_info.h"
#include "ble_manager.h"
#include "button_manager.h"
#include "vending_serial.h"
#include "machine_status_messages.h"
#include "cellular_manager.h"
#include "time_sync.h"
#include <esp_task_wdt.h>

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
static uint32_t g_lastPublishedCounterSequence = 0;
static uint32_t g_lastCounterPublishAttempt = 0;
static bool g_counterPublishAttempted = false;
static bool g_watchdogActive = false;
static uint32_t g_lastPublishedDiagnosticAttempt = 0;
static uint32_t g_lastDiagnosticPublishAt = 0;
static bool g_diagnosticPublishAttempted = false;

static void feedRecoveryWatchdog() {
  if (g_watchdogActive) esp_task_wdt_reset();
}

static void publishCounterDiagnostic(const String& uniqueCode) {
  const uint32_t now = millis();
  if (g_diagnosticPublishAttempted && now - g_lastDiagnosticPublishAt < 1000) return;
  String status, raw, echoRaw, rinseAction;
  bool machineStatus = false;
  uint32_t echoCount = 0;
  uint32_t attempt, finishedAt;
  time_t epoch;
  if (!VendingSerial::latestResult(status, raw, attempt, finishedAt, epoch,
                                  &echoCount, &echoRaw, &machineStatus, &rinseAction) ||
      attempt == g_lastPublishedDiagnosticAttempt) return;
  g_diagnosticPublishAttempted = true;
  g_lastDiagnosticPublishAt = now;
  CounterProtocol::Reading reading;
  uint32_t level = 0, number = 0;
  const char* error = status != "complete" ? status.c_str() :
      (machineStatus
          ? CounterProtocol::parseMachineStatus(raw.c_str(), raw.length(), level, number)
          : CounterProtocol::parse(raw.c_str(), raw.length(), reading));
  JsonDocument doc;
  doc["UniqueCode"] = uniqueCode;
  doc["RequestType"] = machineStatus ? "MachineStatusDiagnostic" : "CounterDiagnostic";
  doc["Command"] = machineStatus ? "MACHINE_STATUS" : "READ_ALL_BOTH_COUNTERS";
  if (machineStatus) {
    doc["SentCommand"] = "*1,E,r,1,0,5B\\r";
    doc["AutoRinseAction"] = rinseAction;
    if (rinseAction == "sent" || rinseAction == "write_failed")
      doc["RinseCommand"] = "*1,D,w,1,100,5E\\r";
    if (!error) {
      doc["ErrorLevel"] = level;
      doc["ErrorNumber"] = number;
      doc["MachineMessage"] = machineStatusMessage(level, number);
    }
  }
  doc["AttemptSequence"] = attempt;
  doc["CapturedAtUptimeMs"] = finishedAt;
  if (epoch) doc["CapturedDateTime"] = TimeSync::format(epoch);
  else doc["CapturedDateTime"] = nullptr;
  doc["Status"] = status;
  doc["ReceivedBytes"] = raw.length();
  doc["EchoCount"] = echoCount;
  doc["EchoBytes"] = echoCount * echoRaw.length();
  doc["RawEcho"] = echoRaw;
  doc["RxPin"] = VendingSerial::activeRxPin();
  doc["TxPin"] = VendingSerial::activeTxPin();
  doc["Valid"] = error == nullptr;
  doc["Partial"] = status == "timeout" && raw.length() > 0;
  if (error) doc["Error"] = error;
  else doc["Error"] = nullptr;
  // Length-aware serialization retains embedded NULs in malformed captures.
  JsonString captured(raw.c_str(), raw.length());
  doc["RawResponse"] = captured;
  String payload;
  serializeJson(doc, payload);
  if (MqttManager::publishJson("godrej/counterdiagnostic/" + uniqueCode, payload)) {
    g_lastPublishedDiagnosticAttempt = attempt;
    g_diagnosticPublishAttempted = false;
  }
}

static void publishLatestCounters(const String& uniqueCode) {
  CounterProtocol::Reading reading;
  uint32_t capturedAt;
  uint32_t sequence;
  time_t capturedEpoch;
  if (!VendingSerial::latestReading(reading, capturedAt, &sequence, &capturedEpoch) ||
      sequence == g_lastPublishedCounterSequence) return;
  const uint32_t now = millis();
  if (g_counterPublishAttempted && now - g_lastCounterPublishAttempt < 1000) return;
  g_lastCounterPublishAttempt = now;
  g_counterPublishAttempted = true;

  MachineInfo info;
  Storage::loadMachineInfo(info);
  JsonDocument doc;
  doc["MachineId"] = info.MachineId;
  doc["UniqueCode"] = uniqueCode;
  doc["RequestType"] = "Counter";
  String temporary;
  String permanent;
  for (size_t i = 0; i < 15; ++i) {
    if (i) { temporary += ','; permanent += ','; }
    temporary += String(reading.temporary[i]);
    permanent += String(reading.permanent[i]);
  }
  doc["TempCount"] = temporary;
  doc["PermanentCount"] = permanent;
  doc["OmittedSlot13"] = reading.omittedSlot13;
  doc["CounterSequence"] = sequence;
  doc["CapturedAtUptimeMs"] = capturedAt;
  if (capturedEpoch) doc["CounterDateTime"] = TimeSync::format(capturedEpoch);
  else doc["CounterDateTime"] = nullptr;
  String payload;
  serializeJson(doc, payload);
  if (MqttManager::publishJson("godrej/sendcounter/" + uniqueCode, payload)) {
    g_lastPublishedCounterSequence = sequence;
    g_counterPublishAttempted = false;
  }
}

static String getCurrentUniqueCode() {
  String uniqueCode;
  if (!Storage::loadUniqueCode(uniqueCode)) {
    return "";
  }
  return uniqueCode;
}

static bool hasCurrentUniqueCode() {
  return getCurrentUniqueCode().length() > 0;
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
  outDateTime = TimeSync::format(TimeSync::now(), "%d/%m/%Y, %H:%M:%S");
  return outDateTime.length() > 0;
}

static String buildStatusDateTime() {
  String statusDateTime;
  if (tryGetDeviceDateTime(statusDateTime)) {
    return statusDateTime;
  }

  return "";
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
  String statusTime = buildStatusDateTime();
  statusData += ",\"StatusDateTime\":";
  statusData += statusTime.length() ? "\"" + statusTime + "\"" : String("null");
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
  TimeSync::begin();
  Serial.println();
  Serial.println("================================");
  Serial.println("SETUP STARTED");
  Serial.println("================================");

  if (!Storage::begin()) {
    Serial.println("Storage initialization failed");
  }
  VendingSerial::begin();

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

#if COUNTER_SERIAL_DIAGNOSTICS
  // No Wi-Fi, cellular, or MQTT startup during the bench capture test.
  // Stored settings are retained. The dedicated UART task handles polling.
  return;
#endif

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
  Serial.print("UniqueCode: ");
  Serial.println(uniqueCode.length() > 0 ? uniqueCode : "<not set>");
  if (uniqueCode.length() > 0) {
    Serial.print("GetInfo: "); Serial.println(topicGetInfo(uniqueCode));
    Serial.print("SendInfo: "); Serial.println(topicSendInfo(uniqueCode));
    Serial.print("SendStatus: "); Serial.println(topicSendStatus(uniqueCode));
  } else {
    Serial.println("GetInfo: <disabled until UniqueCode is set>");
    Serial.println("SendInfo: <disabled until UniqueCode is set>");
    Serial.println("SendStatus: <disabled until UniqueCode is set>");
  }
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
    WiFi.mode(WIFI_OFF);
    CellularManager::begin();
  }

  if (isWifiMode()) {
    TimeSync::startWifiSync();
    mqtt.setClient(wifiClient);
  } else {
    mqtt.setClient(CellularManager::client());
  }

  MqttManager::begin(mqtt, g_mqttHost.c_str(), g_mqttPort,
                     hasCurrentUniqueCode() ? getCurrentUniqueCode() : getChipIdHex(),
                     g_mqttUser.c_str(), g_mqttPass.c_str());
  // Main loop liveness only: network outages do not cause resets. A blocked
  // task does. ESP.restart/watchdog/hardware RESET all preserve NVS settings.
  g_watchdogActive = esp_task_wdt_init(RECOVERY_WATCHDOG_SECONDS, true) == ESP_OK &&
                     esp_task_wdt_add(nullptr) == ESP_OK;
  if (g_watchdogActive) VendingSerial::enableWatchdog();
}

void loop() {
  feedRecoveryWatchdog();
  VendingSerial::setPollingEnabled(!BLEManager::isMaintenanceMode());
#if COUNTER_SERIAL_DIAGNOSTICS
  ButtonManager::loop();
  BLEManager::loop();
  delay(10);
  return;
#endif
  static bool uniqueCodeMissingPrinted = false;
  static bool maintenancePauseApplied = false;

  if (BLEManager::isMaintenanceMode()) {
    if (!maintenancePauseApplied) {
      Serial.println("Maintenance mode active. Pausing WiFi/MQTT until reboot.");
      MqttManager::disconnect();
      if (isWifiMode()) WiFiManager::disconnect();
      else CellularManager::disconnect();
      topicsSubscribed = false;
      infoRequestSent = false;
      bootHeartbeatSent = false;
      uniqueCodeMissingPrinted = false;
      maintenancePauseApplied = true;
    }

    ButtonManager::loop();
    BLEManager::loop();
    delay(10);
    return;
  }

  maintenancePauseApplied = false;

  if (isWifiMode()) WiFiManager::ensureConnected();
  else CellularManager::ensureConnected();
  feedRecoveryWatchdog();

  if (isWifiMode() ? WiFiManager::isConnected() : CellularManager::isConnected()) {
    if (!hasCurrentUniqueCode()) {
      if (!uniqueCodeMissingPrinted) {
        Serial.println("UniqueCode not set. Waiting for BLE provisioning before MQTT connect.");
        uniqueCodeMissingPrinted = true;
      }
      topicsSubscribed = false;
      ButtonManager::loop();
      BLEManager::loop();
      delay(10);
      return;
    }

    uniqueCodeMissingPrinted = false;
    MqttManager::ensureConnected();
    feedRecoveryWatchdog();

    if (MqttManager::isConnected()) {
      String uniqueCode = getCurrentUniqueCode();
      if (!topicsSubscribed) {
        MqttManager::subscribeTopics(topicGetInfo(uniqueCode), false, 1);
        topicsSubscribed = true;
        infoRequestSent = false;
        bootHeartbeatSent = false;
        // Skip snapshots captured offline and request a fresh one for this
        // connection. An already pending read may complete after reconnect.
        g_lastPublishedCounterSequence = VendingSerial::readingSequence();
        g_counterPublishAttempted = false;
        VendingSerial::requestCounterRead();
      }

      MqttManager::loop();
      feedRecoveryWatchdog();
      publishLatestCounters(uniqueCode);
      feedRecoveryWatchdog();
      publishCounterDiagnostic(uniqueCode);
      feedRecoveryWatchdog();

      if (!bootHeartbeatSent) {
        String hb = buildBootHeartbeatStatusJson();
        bool ok = MqttManager::publishJson(topicSendStatus(uniqueCode), hb);
        feedRecoveryWatchdog();

        Serial.print("Published Boot Heartbeat -> ");
        Serial.print(topicSendStatus(uniqueCode));
        Serial.print(" ok=");
        Serial.println(ok ? "1" : "0");
        Serial.println(hb);

        bootHeartbeatSent = ok;
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
        feedRecoveryWatchdog();

        Serial.print("Published Info Request -> ");
        Serial.print(topicSendInfo(uniqueCode));
        Serial.print(" ok=");
        Serial.println(ok ? "1" : "0");
        Serial.println(payload);

        infoRequestSent = ok;
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
