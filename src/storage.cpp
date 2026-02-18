#include "storage.h"
#include <Preferences.h>

static Preferences prefs;

namespace Storage {

    void begin(const char* ns) {
    prefs.begin(ns, false); // read/write
    }

    bool saveUniqueCode(const String& uniqueCode) {
    return prefs.putString("unique", uniqueCode) > 0;
    }

    bool loadUniqueCode(String& outUniqueCode) {
    outUniqueCode = prefs.getString("unique", "");
    return outUniqueCode.length() > 0;
    }

    bool saveMachineInfo(const MachineInfo& info) {
    // Save only what you need long-term
    prefs.putBool("valid", info.valid);
    prefs.putInt("mid", info.MachineId);
    prefs.putInt("cid", info.CompanyId);
    prefs.putInt("lid", info.LocationId);
    prefs.putInt("ssid", info.ScreenSaverId);
    prefs.putInt("msi", info.MachineStatusInterval);
    prefs.putInt("mri", info.MachineReadCounterInterval);

    prefs.putString("ecode", info.ErrorCode);
    prefs.putString("emsg", info.ErrorMessage);
    prefs.putString("rtype", info.RequestType);

    prefs.putString("mname", info.MachineName);
    prefs.putString("cname", info.CompanyName);
    prefs.putString("lname", info.LocationName);

    prefs.putString("model", info.ModelNo);
    prefs.putString("serial", info.SerialNo);
    prefs.putString("asset", info.AssetNo);
    prefs.putString("sw", info.SoftwareVersion);
    prefs.putString("inst", info.InstallationDate);

    prefs.putString("video", info.VideoURL);

    return true;
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

    void clearAll() {
    prefs.clear();
    }

    bool isProvisioned() {
    bool valid = prefs.getBool("valid", false);
    int mid = prefs.getInt("mid", 0);
    return valid && mid > 0;
    }

    void factoryReset(bool reboot) {
        Serial.println("⚠️ FACTORY RESET STARTED");
        Serial.println("Clearing NVS storage...");
        prefs.clear();
        Serial.println("✅ NVS cleared successfully");

        if (reboot) {
            Serial.println("🔁 Rebooting device...");
            delay(500);
            ESP.restart();
        }
    }
    
    void saveWiFi(const String& ssid, const String& pass) {
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_pass", pass);
        }

    bool loadWiFi(String& outSsid, String& outPass) {
        outSsid = prefs.getString("wifi_ssid", "");
        outPass = prefs.getString("wifi_pass", "");

        // valid only if SSID exists
        return outSsid.length() > 0;
    }

} // namespace
