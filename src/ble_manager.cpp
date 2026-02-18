#include "ble_manager.h"
#include "storage.h"
#include "machine_info.h"

#include <NimBLEDevice.h>
#include "esp_system.h"
#include "esp_bt.h"


static NimBLECharacteristic* g_tx = nullptr;
static bool g_connected = false;

static String buildStatusJson() {
  MachineInfo info;
  bool hasInfo = Storage::loadMachineInfo(info);

  String json = "{";
  json += "\"provisioned\":"; json += (Storage::isProvisioned() ? "true" : "false");
  json += ",\"hasMachineInfo\":"; json += (hasInfo ? "true" : "false");
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

static void handleSetWiFi(const String& cmd) {
  int colon = cmd.indexOf(':');
  int pipe  = cmd.indexOf('|');

  if (colon < 0 || pipe < 0 || pipe <= colon) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Invalid SET_WIFI format\"}");
    return;
  }

  String ssid = cmd.substring(colon + 1, pipe);
  String pass = cmd.substring(pipe + 1);

  ssid.trim();
  pass.trim();

  if (ssid.length() == 0) {
    BLEManager::notifyText("{\"ok\":false,\"msg\":\"SSID empty\"}");
    return;
  }

  Serial.print("BLE SET_WIFI SSID: ");
  Serial.println(ssid);

  // Save to NVS
  Storage::saveWiFi(ssid, pass);

  BLEManager::notifyText("{\"ok\":true,\"msg\":\"WiFi saved. Rebooting\"}");

  delay(300);
  ESP.restart();
}


class ServerCB : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s)  {
    g_connected = true;
    Serial.println("BLE connected ✅");
  }
  void onDisconnect(NimBLEServer* s)  {
    g_connected = false;
    Serial.println("BLE disconnected");
    NimBLEDevice::startAdvertising();
  }
};

class RxCB : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c)  {
    std::string v = c->getValue();
    String cmd = String(v.c_str());
    cmd.trim();

    Serial.print("BLE RX: ");
    Serial.println(cmd);

    if (cmd.equalsIgnoreCase("GET")) {
      BLEManager::notifyText(buildStatusJson());
      return;
    }

    if (cmd.equalsIgnoreCase("REBOOT")) {
      BLEManager::notifyText("{\"ok\":true,\"msg\":\"Rebooting\"}");
      delay(200);
      ESP.restart();
      return;
    }

    if (cmd.equalsIgnoreCase("RESET")) {
      BLEManager::notifyText("{\"ok\":true,\"msg\":\"Factory reset\"}");
      delay(200);
      Storage::factoryReset(true); // clears NVS + reboots
      return;
    }
    if (cmd.startsWith("SET_WIFI:")) {
      handleSetWiFi(cmd);
      return;
    }

    BLEManager::notifyText("{\"ok\":false,\"msg\":\"Unknown cmd\"}");
  }
};

namespace BLEManager {

void begin(const String& deviceName) {
  NimBLEDevice::init(deviceName.c_str());
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCB());

  // Nordic UART style UUIDs
  NimBLEService* svc = server->createService("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");

  // RX (Write)
  NimBLECharacteristic* rx = svc->createCharacteristic(
    "6E400002-B5A3-F393-E0A9-E50E24DCCA9E",
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
  );
  rx->setCallbacks(new RxCB());

  // TX (Notify)
  g_tx = svc->createCharacteristic(
    "6E400003-B5A3-F393-E0A9-E50E24DCCA9E",
    NIMBLE_PROPERTY::NOTIFY
  );

  svc->start();

//   NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
//   adv->addServiceUUID(svc->getUUID());
//   adv->start();

  
//   Serial.println("BLE started ✅");
NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();

// ---------- Advertisement data ----------
NimBLEAdvertisementData advData;
advData.setName(deviceName.c_str());
// advData.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
advData.addServiceUUID(svc->getUUID());

// ---------- Scan response data ----------
NimBLEAdvertisementData scanResp;
scanResp.setName(deviceName.c_str());   // 🔑 Name in scan response

adv->setAdvertisementData(advData);
adv->setScanResponseData(scanResp);

adv->start();

Serial.println("BLE advertising started (adv + scan response)");


}

void loop() {
  // NimBLE runs in background; nothing required here
}

bool isConnected() {
  return g_connected;
}

void notifyText(const String& msg) {
  if (!g_tx) return;
  g_tx->setValue(msg.c_str());
  g_tx->notify();
}

} // namespace
