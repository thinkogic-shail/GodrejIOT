#pragma once
#include <Arduino.h>

// -------- Device Identity --------
static const char* UNIQUE_CODE = "G-IOT";

// -------- WiFi --------
static const char* WIFI_SSID = "OpShailendra";
static const char* WIFI_PASS = "12345678";

// -------- MQTT --------
static const char* MQTT_HOST = "13.126.103.168";
static const uint16_t MQTT_PORT = 1883;

// Topics are derived from UNIQUE_CODE
inline String topicGetInfo() { return "godrej/getinfo/" + String(UNIQUE_CODE); }
inline String topicSendInfo(){ return "godrej/sendinfo/" + String(UNIQUE_CODE); }
inline String topicSendStatus(){ return "godrej/sendstatus/" + String(UNIQUE_CODE); }

// -------- Reset Button (hardware long-press) --------
// NOTE: You must set this to the correct GPIO connected to your button.
// Common options: GPIO0 (BOOT), GPIO12/13/14/27 etc.
static const int RESET_BUTTON_PIN = 0;              // try 0 if BOOT is exposed
static const bool RESET_BUTTON_ACTIVE_LOW = true;   // most buttons pull to GND
static const uint32_t LONG_PRESS_MS = 5000;         // 5 sec = factory reset
static const uint32_t SHORT_PRESS_MS = 500;         // 0.5 sec = reboot
