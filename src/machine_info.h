#pragma once
#include <Arduino.h>

struct MachineInfo {
  bool valid = false;

  String ErrorCode;
  String ErrorMessage;
  String RequestType;

  int MachineId = 0;
  String MachineName;

  int CompanyId = 0;
  String CompanyName;

  int LocationId = 0;
  String LocationName;

  String ModelNo;
  String SerialNo;
  String AssetNo;
  String SoftwareVersion;
  String InstallationDate;

  int ScreenSaverId = 0;
  String VideoURL;

  int MachineStatusInterval = 0;
  int MachineReadCounterInterval = 0;
};

namespace MachineInfoStore {
  void clear();
  bool parseAndStore(const String& jsonArrayPayload); // expects [ { ... } ]
  const MachineInfo& get();
  void printToSerial();
}
