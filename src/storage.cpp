#include "storage.h"
#include "vending_uart_config.h"
#include "config.h"
#include <Preferences.h>

static Preferences prefs;
static constexpr uint16_t STORAGE_SCHEMA_VERSION = 1;
static constexpr const char* KEY_SCHEMA = "schema";

static bool writeString(const char* key, const String& value) {
  return prefs.putString(key, value) == value.length();
}

namespace Storage {

bool begin(const char* ns) {
  if (!prefs.begin(ns, false)) {
    Serial.println("Storage begin failed");
    return false;
  }

  uint16_t storedVersion = prefs.getUShort(KEY_SCHEMA, 0);
  if (storedVersion == 0) {
    if (prefs.putUShort(KEY_SCHEMA, STORAGE_SCHEMA_VERSION) != sizeof(uint16_t)) {
      Serial.println("Storage schema initialization failed");
      return false;
    }
    return true;
  }

  if (storedVersion != STORAGE_SCHEMA_VERSION) {
    Serial.print("Storage schema mismatch. Stored=");
    Serial.print(storedVersion);
    Serial.print(" Current=");
    Serial.println(STORAGE_SCHEMA_VERSION);

    bool cleared = prefs.clear();
    bool versionWritten = prefs.putUShort(KEY_SCHEMA, STORAGE_SCHEMA_VERSION) == sizeof(uint16_t);
    if (!cleared || !versionWritten) {
      Serial.println("Storage schema reset failed");
      return false;
    }
  }

  return true;
}

bool saveUniqueCode(const String& uniqueCode) {
  return writeString("unique", uniqueCode);
}

bool loadUniqueCode(String& outUniqueCode) {
  if (!prefs.isKey("unique")) {
    outUniqueCode = "";
    return false;
  }

  outUniqueCode = prefs.getString("unique", "");
  return outUniqueCode.length() > 0;
}

bool saveMachineInfo(const MachineInfo& info) {
  bool ok = true;
  ok &= prefs.putBool("valid", info.valid) == 1;
  ok &= prefs.putInt("mid", info.MachineId) == sizeof(int);
  ok &= prefs.putInt("cid", info.CompanyId) == sizeof(int);
  ok &= prefs.putInt("lid", info.LocationId) == sizeof(int);
  ok &= prefs.putInt("ssid", info.ScreenSaverId) == sizeof(int);
  ok &= prefs.putInt("msi", info.MachineStatusInterval) == sizeof(int);
  ok &= prefs.putInt("mri", info.MachineReadCounterInterval) == sizeof(int);

  ok &= writeString("ecode", info.ErrorCode);
  ok &= writeString("emsg", info.ErrorMessage);
  ok &= writeString("rtype", info.RequestType);
  ok &= writeString("mname", info.MachineName);
  ok &= writeString("cname", info.CompanyName);
  ok &= writeString("lname", info.LocationName);
  ok &= writeString("model", info.ModelNo);
  ok &= writeString("serial", info.SerialNo);
  ok &= writeString("asset", info.AssetNo);
  ok &= writeString("sw", info.SoftwareVersion);
  ok &= writeString("inst", info.InstallationDate);
  ok &= writeString("video", info.VideoURL);

  if (!ok) {
    Serial.println("Storage saveMachineInfo failed");
  }
  return ok;
}

bool loadMachineInfo(MachineInfo& outInfo) {
  bool valid = prefs.getBool("valid", false);
  int mid = prefs.getInt("mid", 0);

  if (!valid || mid <= 0) return false;

  outInfo.valid = valid;
  outInfo.MachineId = mid;
  outInfo.CompanyId = prefs.getInt("cid", 0);
  outInfo.LocationId = prefs.getInt("lid", 0);
  outInfo.ScreenSaverId = prefs.getInt("ssid", 0);
  outInfo.MachineStatusInterval = prefs.getInt("msi", 0);
  outInfo.MachineReadCounterInterval = prefs.getInt("mri", 0);

  outInfo.ErrorCode = prefs.getString("ecode", "");
  outInfo.ErrorMessage = prefs.getString("emsg", "");
  outInfo.RequestType = prefs.getString("rtype", "");
  outInfo.MachineName = prefs.getString("mname", "");
  outInfo.CompanyName = prefs.getString("cname", "");
  outInfo.LocationName = prefs.getString("lname", "");
  outInfo.ModelNo = prefs.getString("model", "");
  outInfo.SerialNo = prefs.getString("serial", "");
  outInfo.AssetNo = prefs.getString("asset", "");
  outInfo.SoftwareVersion = prefs.getString("sw", "");
  outInfo.InstallationDate = prefs.getString("inst", "");
  outInfo.VideoURL = prefs.getString("video", "");

  return true;
}

bool clearAll() {
  bool cleared = prefs.clear();
  bool versionWritten = prefs.putUShort(KEY_SCHEMA, STORAGE_SCHEMA_VERSION) == sizeof(uint16_t);
  return cleared && versionWritten;
}

bool isProvisioned() {
  bool valid = prefs.getBool("valid", false);
  int mid = prefs.getInt("mid", 0);
  return valid && mid > 0;
}

void factoryReset(bool reboot) {
  Serial.println("FACTORY RESET STARTED");
  Serial.println("Clearing NVS storage...");
  bool ok = clearAll();
  Serial.println(ok ? "NVS cleared successfully" : "NVS clear failed");

  if (reboot) {
    Serial.println("Rebooting device...");
    delay(500);
    ESP.restart();
  }
}

bool saveWiFi(const String& ssid, const String& pass) {
  bool ok = writeString("wifi_ssid", ssid);
  ok &= writeString("wifi_pass", pass);
  if (!ok) {
    Serial.println("Storage saveWiFi failed");
  }
  return ok;
}

bool loadWiFi(String& outSsid, String& outPass) {
  size_t ssidLen = prefs.getString("wifi_ssid", nullptr, 0);
  if (ssidLen == 0) {
    outSsid = "";
    outPass = "";
    return false;
  }

  outSsid = prefs.getString("wifi_ssid", "");
  outPass = prefs.getString("wifi_pass", "");
  return true;
}

bool saveNetworkMode(const String& mode) {
  return writeString("net_mode", mode);
}

bool loadNetworkMode(String& outMode) {
  size_t modeLen = prefs.getString("net_mode", nullptr, 0);
  if (modeLen == 0) {
    outMode = "";
    return false;
  }

  outMode = prefs.getString("net_mode", "");
  return outMode.length() > 0;
}

bool saveMqttSettings(const String& host, uint16_t port, const String& user, const String& pass) {
  bool ok = writeString("mqtt_host", host);
  ok &= prefs.putUShort("mqtt_port", port) == sizeof(uint16_t);
  ok &= writeString("mqtt_user", user);
  ok &= writeString("mqtt_pass", pass);
  if (!ok) {
    Serial.println("Storage saveMqttSettings failed");
  }
  return ok;
}

bool loadMqttSettings(String& outHost, uint16_t& outPort, String& outUser, String& outPass) {
  size_t hostLen = prefs.getString("mqtt_host", nullptr, 0);
  if (hostLen == 0) {
    outHost = "";
    outPort = 0;
    outUser = "";
    outPass = "";
    return false;
  }

  outHost = prefs.getString("mqtt_host", "");
  outPort = prefs.getUShort("mqtt_port", 0);
  outUser = prefs.getString("mqtt_user", "");
  outPass = prefs.getString("mqtt_pass", "");
  return outHost.length() > 0 && outPort > 0;
}

bool saveVendingUart(int rx, int tx) {
  if (!VendingUartConfig::valid(rx, tx)) return false;
  // One NVS value commits the pair together.
  return prefs.putUInt("vend_uart", (static_cast<uint32_t>(rx) << 8) | tx) == sizeof(uint32_t);
}

bool loadVendingUart(int& rx, int& tx) {
  if (!prefs.isKey("vend_uart")) return false;
  uint32_t packed = prefs.getUInt("vend_uart", UINT32_MAX);
  int savedRx = (packed >> 8) & 255, savedTx = packed & 255;
  if (packed > 65535 || !VendingUartConfig::valid(savedRx, savedTx)) return false;
  rx = savedRx;
  tx = savedTx;
  return true;
}

bool saveSimSettings(const String& apn, const String& user, const String& pass, const String& pin) {
  bool ok = writeString("sim_apn", apn);
  ok &= writeString("sim_user", user);
  ok &= writeString("sim_pass", pass);
  ok &= writeString("sim_pin", pin);
  return ok;
}

bool loadSimSettings(String& apn, String& user, String& pass, String& pin) {
  if (!prefs.isKey("sim_apn")) return false;
  apn = prefs.getString("sim_apn", "");
  user = prefs.getString("sim_user", "");
  pass = prefs.getString("sim_pass", "");
  pin = prefs.getString("sim_pin", "");
  return true;
}
} // namespace
