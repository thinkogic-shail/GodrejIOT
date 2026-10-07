#include "ble_manager.h"
#include "config.h"
#include "storage.h"
#include "machine_info.h"

#include <NimBLEDevice.h>
#include "esp_system.h"
#include "esp_bt.h"

static NimBLECharacteristic* g_tx = nullptr;
static NimBLEServer* g_server = nullptr;
static bool g_connected = false;
static bool g_maintenanceMode = false;
static constexpr uint16_t INVALID_CONN_HANDLE = 0xFFFF;
static uint16_t g_connHandle = INVALID_CONN_HANDLE;

static String loadNetworkModeOrDefault() {
  String mode;
  if (!Storage::loadNetworkMode(mode) || mode.length() == 0) {
    mode = DEFAULT_NETWORK_MODE;
  }
  return mode;
}

static String loadUniqueCodeOrDefault() {
  String uniqueCode;
  if (!Storage::loadUniqueCode(uniqueCode)) {
    uniqueCode = "";
  }
  return uniqueCode;
}

static void loadWiFiOrDefault(String& ssid, String& pass) {
  if (!Storage::loadWiFi(ssid, pass)) {
    ssid = WIFI_SSID;
    pass = WIFI_PASS;
  }
}

static void loadMqttOrDefault(String& host, uint16_t& port, String& user, String& pass) {
  if (!Storage::loadMqttSettings(host, port, user, pass)) {
    host = MQTT_HOST;
    port = MQTT_PORT;
    user = MQTT_USER;
    pass = MQTT_PASS;
  }
}

static String buildStatusJson() {
  MachineInfo info;
  bool hasInfo = Storage::loadMachineInfo(info);

  String json = "{";
  json += "\"provisioned\":";
  json += (Storage::isProvisioned() ? "true" : "false");
  json += ",\"maintenanceMode\":";
  json += (g_maintenanceMode ? "true" : "false");
  json += ",\"hasMachineInfo\":";
  json += (hasInfo ? "true" : "false");
  if (hasInfo) {
    json += ",\"MachineId\":" + String(info.MachineId);
    json += ",\"MachineName\":\"" + info.MachineName + "\"";
    json += ",\"CompanyName\":\"" + info.CompanyName + "\"";
    json += ",\"LocationName\":\"" + info.LocationName + "\"";
    json += ",\"VideoURL\":\"" + info.VideoURL + "\"";
  }
  json += "}";
  return json;
}

static String buildSettingsJson() {
  String uniqueCode = loadUniqueCodeOrDefault();
  String mode = loadNetworkModeOrDefault();
  String wifiSsid;
  String wifiPass;
  String mqttHost;
  String mqttUser;
  String mqttPass;
  uint16_t mqttPort = 0;

  loadWiFiOrDefault(wifiSsid, wifiPass);
  loadMqttOrDefault(mqttHost, mqttPort, mqttUser, mqttPass);

  String json = "{";
  json += "\"uniqueCode\":\"" + uniqueCode + "\"";
  json += ",\"maintenanceMode\":";
  json += (g_maintenanceMode ? "true" : "false");
  json += ",\"mode\":\"" + mode + "\"";
  json += ",\"wifiSsid\":\"" + wifiSsid + "\"";
  json += ",\"wifiPassword\":\"" + wifiPass + "\"";
  json += ",\"mqttHost\":\"" + mqttHost + "\"";
  json += ",\"mqttPort\":\"" + String(mqttPort) + "\"";
  json += ",\"mqttUsername\":\"" + mqttUser + "\"";
  json += ",\"mqttPassword\":\"" + mqttPass + "\"";
  json += "}";
  return json;
}

static void startAdvertisingWithLog(const char* reason) {
  Serial.print("BLE advertising start: ");
  Serial.println(reason);
  NimBLEDevice::startAdvertising();
}

static bool ensureMaintenanceMode(const char* commandName) {
  if (g_maintenanceMode) {
    return true;
  }

  String msg = String("{\"ok\":false,\"msg\":\"") + commandName +
               " requires MAINTENANCE_ON\"}";
  Serial.print("BLE TX: ");
  Serial.println(msg);
  BLEManager::notifyText(msg);
  return false;
}

static bool parseModeValue(const String& inputMode, String& outMode) {
  outMode = inputMode;
  outMode.trim();
  outMode.toUpperCase();
  return outMode == "WIFI" || outMode == "SIM";
}

static bool saveUniqueCodeValue(const String& uniqueCode) {
  if (uniqueCode.length() == 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"UniqueCode empty\"}");
    return false;
  }

  if (!Storage::saveUniqueCode(uniqueCode)) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"UniqueCode save failed\"}");
    return false;
  }

  return true;
}

static bool saveModeValue(const String& mode) {
  if (!Storage::saveNetworkMode(mode)) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Mode save failed\"}");
    return false;
  }

  return true;
}

static bool saveWiFiValue(const String& ssid, const String& pass) {
  if (ssid.length() == 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"SSID empty\"}");
    return false;
  }

  if (!Storage::saveWiFi(ssid, pass)) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"WiFi save failed\"}");
    return false;
  }

  return true;
}

static bool saveMqttValue(const String& host, uint16_t port, const String& user, const String& pass) {
  if (host.length() == 0 || port == 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"MQTT host/port invalid\"}");
    return false;
  }

  if (!Storage::saveMqttSettings(host, port, user, pass)) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"MQTT save failed\"}");
    return false;
  }

  return true;
}

static void releaseBleStack() {
  // Keep pre-restart teardown lightweight. Full NimBLE deinit from a BLE
  // callback can block before ESP.restart() gets a chance to run.
  Serial.println("BLE advertising stop: release before restart");
  NimBLEDevice::stopAdvertising();

  g_tx = nullptr;
  g_connected = false;
  g_connHandle = INVALID_CONN_HANDLE;
}

static void handleSetUniqueCode(const String& cmd) {
  if (!ensureMaintenanceMode("SET_UNIQUE_CODE")) {
    return;
  }

  int colon = cmd.indexOf(':');
  if (colon < 0) {
    Serial.println("BLE TX: {\"ok\":false,\"msg\":\"Invalid SET_UNIQUE_CODE format\"}");
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid SET_UNIQUE_CODE format\"}");
    return;
  }

  String uniqueCode = cmd.substring(colon + 1);
  uniqueCode.trim();
  Serial.print("BLE SET_UNIQUE_CODE: ");
  Serial.println(uniqueCode);

  if (!saveUniqueCodeValue(uniqueCode)) {
    return;
  }

  BLEManager::notifyText("{\"ok\":true,\"msg\":\"UniqueCode saved. Reboot required\"}");
}

static void handleSetMode(const String& cmd) {
  if (!ensureMaintenanceMode("SET_MODE")) {
    return;
  }

  int colon = cmd.indexOf(':');
  if (colon < 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid SET_MODE format\"}");
    return;
  }

  String mode;
  if (!parseModeValue(cmd.substring(colon + 1), mode)) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Mode must be WIFI or SIM\"}");
    return;
  }

  if (!saveModeValue(mode)) {
    return;
  }

  BLEManager::notifyText("{\"ok\":true,\"msg\":\"Mode saved. Reboot required\"}");
}

static void handleSetWiFi(const String& cmd) {
  if (!ensureMaintenanceMode("SET_WIFI")) {
    return;
  }

  int colon = cmd.indexOf(':');
  int pipe = cmd.indexOf('|');

  if (colon < 0 || pipe < 0 || pipe <= colon) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid SET_WIFI format\"}");
    return;
  }

  String ssid = cmd.substring(colon + 1, pipe);
  String pass = cmd.substring(pipe + 1);
  ssid.trim();
  pass.trim();

  Serial.print("BLE SET_WIFI SSID: ");
  Serial.println(ssid);

  if (!saveWiFiValue(ssid, pass)) {
    return;
  }

  BLEManager::notifyText("{\"ok\":true,\"msg\":\"WiFi saved. Reboot required\"}");
}

static void handleSetMqtt(const String& cmd) {
  if (!ensureMaintenanceMode("SET_MQTT")) {
    return;
  }

  int colon = cmd.indexOf(':');
  if (colon < 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid SET_MQTT format\"}");
    return;
  }

  String payload = cmd.substring(colon + 1);
  int p1 = payload.indexOf('|');
  int p2 = (p1 >= 0) ? payload.indexOf('|', p1 + 1) : -1;
  int p3 = (p2 >= 0) ? payload.indexOf('|', p2 + 1) : -1;
  if (p1 < 0 || p2 < 0 || p3 < 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid SET_MQTT format\"}");
    return;
  }

  String host = payload.substring(0, p1);
  String portText = payload.substring(p1 + 1, p2);
  String user = payload.substring(p2 + 1, p3);
  String pass = payload.substring(p3 + 1);
  host.trim();
  portText.trim();
  user.trim();
  pass.trim();

  uint16_t port = (uint16_t)portText.toInt();
  if (!saveMqttValue(host, port, user, pass)) {
    return;
  }

  BLEManager::notifyText("{\"ok\":true,\"msg\":\"MQTT saved. Reboot required\"}");
}

static void handleUpdateAll(const String& cmd) {
  if (!ensureMaintenanceMode("UPDATE_ALL")) {
    return;
  }

  int colon = cmd.indexOf(':');
  if (colon < 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid UPDATE_ALL format\"}");
    return;
  }

  String payload = cmd.substring(colon + 1);
  int p1 = payload.indexOf('|');
  int p2 = (p1 >= 0) ? payload.indexOf('|', p1 + 1) : -1;
  int p3 = (p2 >= 0) ? payload.indexOf('|', p2 + 1) : -1;
  int p4 = (p3 >= 0) ? payload.indexOf('|', p3 + 1) : -1;
  int p5 = (p4 >= 0) ? payload.indexOf('|', p4 + 1) : -1;
  int p6 = (p5 >= 0) ? payload.indexOf('|', p5 + 1) : -1;
  int p7 = (p6 >= 0) ? payload.indexOf('|', p6 + 1) : -1;

  if (p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0 || p5 < 0 || p6 < 0 || p7 < 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid UPDATE_ALL format\"}");
    return;
  }

  String uniqueCode = payload.substring(0, p1);
  String modeText = payload.substring(p1 + 1, p2);
  String wifiSsid = payload.substring(p2 + 1, p3);
  String wifiPass = payload.substring(p3 + 1, p4);
  String mqttHost = payload.substring(p4 + 1, p5);
  String mqttPortText = payload.substring(p5 + 1, p6);
  String mqttUser = payload.substring(p6 + 1, p7);
  String mqttPass = payload.substring(p7 + 1);

  uniqueCode.trim();
  modeText.trim();
  wifiSsid.trim();
  wifiPass.trim();
  mqttHost.trim();
  mqttPortText.trim();
  mqttUser.trim();
  mqttPass.trim();

  String mode;
  if (!parseModeValue(modeText, mode)) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Mode must be WIFI or SIM\"}");
    return;
  }

  uint16_t mqttPort = (uint16_t)mqttPortText.toInt();

  if (!saveUniqueCodeValue(uniqueCode)) return;
  if (!saveModeValue(mode)) return;
  if (!saveWiFiValue(wifiSsid, wifiPass)) return;
  if (!saveMqttValue(mqttHost, mqttPort, mqttUser, mqttPass)) return;

  BLEManager::notifyText("{\"ok\":true,\"msg\":\"All settings saved. Reboot required\"}");
}

static void handleDisconnect() {
  BLEManager::notifyText("{\"status\":\"disconnecting\"}");

  if (!g_server || g_connHandle == INVALID_CONN_HANDLE) {
    startAdvertisingWithLog("disconnect requested without active connection");
    return;
  }

  delay(100);
  if (!g_server->disconnect(g_connHandle)) {
    Serial.println("BLE disconnect request failed");
    startAdvertisingWithLog("disconnect failed, resuming advertising");
  }
}

class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& connInfo) override {
    g_connected = true;
    g_server = server;
    g_connHandle = connInfo.getConnHandle();
    Serial.print("BLE connected handle=");
    Serial.println(g_connHandle);
  }

  void onDisconnect(NimBLEServer* server, NimBLEConnInfo& connInfo, int reason) override {
    (void)connInfo;
    (void)reason;
    g_connected = false;
    g_connHandle = INVALID_CONN_HANDLE;
    Serial.println("BLE disconnected");
    Serial.println("BLE advertising start: client disconnected");
    server->startAdvertising();
  }
};

class RxCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {
    g_connHandle = connInfo.getConnHandle();
    std::string value = c->getValue();
    String cmd = String(value.c_str());
    cmd.trim();

    Serial.print("BLE RX: ");
    Serial.println(cmd);

    if (cmd.equalsIgnoreCase("GET")) {
      BLEManager::notifyText(buildStatusJson());
      return;
    }

    if (cmd.equalsIgnoreCase("GET_SETTINGS")) {
      String settingsJson = buildSettingsJson();
      Serial.print("BLE TX: ");
      Serial.println(settingsJson);
      BLEManager::notifyText(settingsJson);
      return;
    }

    if (cmd.equalsIgnoreCase("READ_ALL")) {
      String settingsJson = buildSettingsJson();
      Serial.print("BLE TX: ");
      Serial.println(settingsJson);
      BLEManager::notifyText(settingsJson);
      return;
    }

    if (cmd.equalsIgnoreCase("MAINTENANCE_ON")) {
      g_maintenanceMode = true;
      Serial.println("BLE maintenance mode: ON");
      BLEManager::notifyText("{\"ok\":true,\"msg\":\"Maintenance mode enabled\"}");
      return;
    }

    if (cmd.equalsIgnoreCase("DISCONNECT") || cmd.equalsIgnoreCase("BYE")) {
      handleDisconnect();
      return;
    }

    if (cmd.startsWith("SET_UNIQUE_CODE:")) {
      handleSetUniqueCode(cmd);
      return;
    }

    if (cmd.startsWith("UPDATE_ALL:")) {
      handleUpdateAll(cmd);
      return;
    }

    if (cmd.equalsIgnoreCase("REBOOT")) {
      BLEManager::notifyText("{\"ok\":true,\"msg\":\"Rebooting\"}");
      delay(200);
      BLEManager::release();
      ESP.restart();
      return;
    }

    if (cmd.equalsIgnoreCase("RESET")) {
      BLEManager::notifyText("{\"ok\":true,\"msg\":\"Factory reset\"}");
      delay(200);
      BLEManager::release();
      Storage::factoryReset(true);
      return;
    }

    if (cmd.startsWith("SET_MODE:")) {
      handleSetMode(cmd);
      return;
    }

    if (cmd.startsWith("SET_WIFI:")) {
      handleSetWiFi(cmd);
      return;
    }

    if (cmd.startsWith("SET_MQTT:")) {
      handleSetMqtt(cmd);
      return;
    }

    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Unknown cmd\"}");
  }
};

namespace BLEManager {

void begin(const String& deviceName) {
  Serial.println("BLE init: releasing classic BT memory");
  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);

  delay(100);
  Serial.println("BLE init: NimBLEDevice::init");
  NimBLEDevice::init(deviceName.c_str());
  NimBLEDevice::setPower(ESP_PWR_LVL_N12);

  Serial.println("BLE init: creating server");
  NimBLEServer* server = NimBLEDevice::createServer();
  g_server = server;
  server->setCallbacks(new ServerCB());

  NimBLEService* svc = server->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");

  NimBLECharacteristic* rx = svc->createCharacteristic(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  rx->setCallbacks(new RxCB());

  g_tx = svc->createCharacteristic(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
    NIMBLE_PROPERTY::NOTIFY
  );

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setName(deviceName.c_str());
  advData.addServiceUUID(svc->getUUID());

  NimBLEAdvertisementData scanResp;
  scanResp.setName(deviceName.c_str());

  adv->setAdvertisementData(advData);
  adv->setScanResponseData(scanResp);

  Serial.println("BLE init: starting advertising");
  adv->start();
  Serial.println("BLE advertising started (adv + scan response)");
}

void loop() {
}

bool isConnected() {
  return g_connected;
}

bool isMaintenanceMode() {
  return g_maintenanceMode;
}

void notifyText(const String& msg) {
  if (!g_tx) return;
  g_tx->setValue(msg.c_str());
  g_tx->notify();
}

void release() {
  if (!g_server && !g_tx && g_connHandle == INVALID_CONN_HANDLE) {
    return;
  }

  Serial.println("BLE release: stopping BLE before restart");
  releaseBleStack();
}

} // namespace
