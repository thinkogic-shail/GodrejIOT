#include "machine_info.h"
#include <ArduinoJson.h>

static MachineInfo g_machine;

namespace MachineInfoStore {

void clear() {
  g_machine = MachineInfo();
}

const MachineInfo& get() {
  return g_machine;
}

void printToSerial() {
  Serial.println("\n=========== STORED MACHINE INFO ===========");
  Serial.printf("valid: %s\n", g_machine.valid ? "true" : "false");

  Serial.printf("ErrorCode: %s\n", g_machine.ErrorCode.c_str());
  Serial.printf("ErrorMessage: %s\n", g_machine.ErrorMessage.c_str());
  Serial.printf("RequestType: %s\n", g_machine.RequestType.c_str());

  Serial.printf("MachineId: %d\n", g_machine.MachineId);
  Serial.printf("MachineName: %s\n", g_machine.MachineName.c_str());

  Serial.printf("CompanyId: %d\n", g_machine.CompanyId);
  Serial.printf("CompanyName: %s\n", g_machine.CompanyName.c_str());

  Serial.printf("LocationId: %d\n", g_machine.LocationId);
  Serial.printf("LocationName: %s\n", g_machine.LocationName.c_str());

  Serial.printf("ModelNo: %s\n", g_machine.ModelNo.c_str());
  Serial.printf("SerialNo: %s\n", g_machine.SerialNo.c_str());
  Serial.printf("AssetNo: %s\n", g_machine.AssetNo.c_str());
  Serial.printf("SoftwareVersion: %s\n", g_machine.SoftwareVersion.c_str());
  Serial.printf("InstallationDate: %s\n", g_machine.InstallationDate.c_str());

  Serial.printf("ScreenSaverId: %d\n", g_machine.ScreenSaverId);
  Serial.printf("VideoURL: %s\n", g_machine.VideoURL.c_str());

  Serial.printf("MachineStatusInterval: %d\n", g_machine.MachineStatusInterval);
  Serial.printf("MachineReadCounterInterval: %d\n", g_machine.MachineReadCounterInterval);

  Serial.println("===========================================");
}

bool parseAndStore(const String& jsonArrayPayload) {
  JsonDocument doc;
  auto err = deserializeJson(doc, jsonArrayPayload);
  if (err) {
    Serial.print("JSON parse failed: ");
    Serial.println(err.c_str());
    Serial.println("Raw payload was:");
    Serial.println(jsonArrayPayload);

    return false;
  }

  if (!doc.is<JsonArray>()) {
    Serial.println("JSON is not an array!");
    return false;
  }

  JsonArray arr = doc.as<JsonArray>();
  if (arr.size() == 0) {
    Serial.println("JSON array empty!");
    return false;
  }

  JsonObject o = arr[0];

  g_machine.ErrorCode = (const char*)(o["ErrorCode"] | "");
  g_machine.ErrorMessage = (const char*)(o["ErrorMessage"] | "");
  g_machine.RequestType = (const char*)(o["RequestType"] | "");

  g_machine.MachineId = (int)(o["MachineId"] | 0);
  g_machine.MachineName = (const char*)(o["MachineName"] | "");

  g_machine.CompanyId = (int)(o["CompanyId"] | 0);
  g_machine.CompanyName = (const char*)(o["CompanyName"] | "");

  g_machine.LocationId = (int)(o["LocationId"] | 0);
  g_machine.LocationName = (const char*)(o["LocationName"] | "");

  g_machine.ModelNo = (const char*)(o["ModelNo"] | "");
  g_machine.SerialNo = (const char*)(o["SerialNo"] | "");
  g_machine.AssetNo = (const char*)(o["AssetNo"] | "");
  g_machine.SoftwareVersion = (const char*)(o["SoftwareVersion"] | "");
  g_machine.InstallationDate = (const char*)(o["InstallationDate"] | "");

  g_machine.ScreenSaverId = (int)(o["ScreenSaverId"] | 0);
  g_machine.VideoURL = (const char*)(o["VideoURL"] | "");

  g_machine.MachineStatusInterval = (int)(o["MachineStatusInterval"] | 0);
  g_machine.MachineReadCounterInterval = (int)(o["MachineReadCounterInterval"] | 0);

  g_machine.valid = (g_machine.ErrorCode == "00");
  return true;
}

} // namespace
