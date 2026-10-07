#pragma once
#include <Arduino.h>

#ifndef DEBUG_LOGS
#define DEBUG_LOGS 1
#endif

#if !DEBUG_LOGS
class DebugSerialProxy {
 public:
  template <typename... Args>
  void begin(Args...) {}

  size_t println() { return 0; }

  template <typename T>
  size_t print(const T&) { return 0; }

  template <typename T>
  size_t println(const T&) { return 0; }

  template <typename... Args>
  size_t printf(const char*, Args...) { return 0; }
};

inline DebugSerialProxy DebugSerial;
#define Serial DebugSerial
#endif

#if DEBUG_LOGS
#define DBG_BEGIN(...) Serial.begin(__VA_ARGS__)
#define DBG_PRINT(...) Serial.print(__VA_ARGS__)
#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)
#define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)
#else
#define DBG_BEGIN(...)
#define DBG_PRINT(...)
#define DBG_PRINTLN(...)
#define DBG_PRINTF(...)
#endif

// -------- Device Identity --------
static const char* UNIQUE_CODE = "";

// -------- WiFi --------
static const char* WIFI_SSID = "OpShailendra";
static const char* WIFI_PASS = "12345678";
static const char* DEFAULT_NETWORK_MODE = "WIFI";

// -------- MQTT --------
static const char* MQTT_HOST = "13.126.103.168";
static const uint16_t MQTT_PORT = 1883;
static const char* MQTT_USER = "";
static const char* MQTT_PASS = "";

// Topics are derived from UNIQUE_CODE
inline String topicGetInfo() { return "godrej/getinfo/" + String(UNIQUE_CODE); }
inline String topicSendInfo(){ return "godrej/sendinfo/" + String(UNIQUE_CODE); }
inline String topicSendStatus(){ return "godrej/sendstatus/" + String(UNIQUE_CODE); }

// -------- Reset Button (hardware long-press) --------
// NOTE: Set this to the GPIO connected to your button.
// Keep GPIO0 only as a last resort because it is a boot strapping pin on ESP32.
// Use a safer free GPIO such as 12/13/14/27 when available.
static const int RESET_BUTTON_PIN = -1;             // disabled until a safe GPIO is assigned
static const bool RESET_BUTTON_ACTIVE_LOW = true;   // most buttons pull to GND
static const uint32_t LONG_PRESS_MS = 5000;         // 5 sec = factory reset
static const uint32_t SHORT_PRESS_MS = 500;         // 0.5 sec = reboot
