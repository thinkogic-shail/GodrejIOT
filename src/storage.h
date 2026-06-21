#pragma once
#include <Arduino.h>
#include "machine_info.h"

namespace Storage {
  bool begin(const char* ns = "godrej");

  bool saveUniqueCode(const String& uniqueCode);
  bool loadUniqueCode(String& outUniqueCode);

  bool saveMachineInfo(const MachineInfo& info);
  bool loadMachineInfo(MachineInfo& outInfo);

  bool clearAll();

  bool isProvisioned(); // machineInfo.valid == true AND machineId > 0
  void factoryReset(bool reboot = true);

  bool saveWiFi(const String& ssid, const String& pass);
  bool loadWiFi(String& outSsid, String& outPass);   
}
