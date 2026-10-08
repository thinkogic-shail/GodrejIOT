#pragma once
#include <Arduino.h>

#ifndef DEBUG_LOGS
#define DEBUG_LOGS 0
#endif

// Temporary shared-UART diagnostics; enabled only by counter_diagnostic build.
#ifndef COUNTER_SERIAL_DIAGNOSTICS
#define COUNTER_SERIAL_DIAGNOSTICS 0
#endif
#ifndef MACHINE_STATUS_PROBE
#define MACHINE_STATUS_PROBE 0
#endif

#if DEBUG_LOGS
#error "UART0 RX/TX is reserved for the vending machine; serial debug logging must stay disabled."
#endif

// Dedicated vending UART defaults: yellow -> GPIO13 RX, green -> GPIO14 TX.
// Black stays on GND; machine-end wiring is unchanged. BLE can override pins.
static constexpr int VENDING_RX_PIN = 13;
static constexpr int VENDING_TX_PIN = 14;
static constexpr uint32_t VENDING_BAUD_RATE = 9600;
static constexpr size_t VENDING_RX_BUFFER_SIZE = 1024;
static constexpr uint32_t COUNTER_POLL_INTERVAL_MS = 10000;

// Bharat Pi legacy 4G/A7672S: modem UART2 (not the vending UART0).
static constexpr int MODEM_RX_PIN = 16;
static constexpr int MODEM_TX_PIN = 17;
static constexpr int MODEM_POWER_KEY_PIN = 32;
static constexpr uint32_t MODEM_BAUD_RATE = 115200;
// Blank APN preserves the modem/carrier PDP profile. Override via BLE if needed.
static const char* SIM_APN = "";
static const char* SIM_APN_USER = "";
static const char* SIM_APN_PASS = "";
static const char* SIM_PIN = "";
static constexpr uint32_t RECOVERY_WATCHDOG_SECONDS = 60;

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

static DebugSerialProxy DebugSerial;
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

// -------- Optional extra GPIO reboot button --------
// The onboard RESET button already resets EN directly, even if firmware hangs.
// It needs no GPIO assignment and preserves NVS. Do not repurpose BOOT/GPIO0.
// NOTE: Set this to the GPIO connected to your button.
// Keep GPIO0 only as a last resort because it is a boot strapping pin on ESP32.
// Use a safer free GPIO such as 12/13/14/27 when available.
static const int RESET_BUTTON_PIN = -1;             // disabled until a safe GPIO is assigned
static const bool RESET_BUTTON_ACTIVE_LOW = true;   // most buttons pull to GND
static const uint32_t LONG_PRESS_MS = 5000;         // 5 sec = reboot, settings preserved
static const uint32_t SHORT_PRESS_MS = 500;         // 0.5 sec = reboot
