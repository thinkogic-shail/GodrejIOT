#include "wifi_manager.h"
#include <WiFi.h>

static String g_ssid;
static String g_pass;
static uint32_t g_lastAttemptAt = 0;
static wl_status_t g_lastStatus = WL_IDLE_STATUS;
static bool g_attemptInProgress = false;
static bool g_missingCredsLogged = false;

static constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 15000;

namespace WiFiManager {

void begin(const char* ssid, const char* pass) {
  g_ssid = ssid ? ssid : "";
  g_pass = pass ? pass : "";
  g_lastAttemptAt = 0;
  g_lastStatus = WL_IDLE_STATUS;
  g_attemptInProgress = false;
  g_missingCredsLogged = false;

  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  // BLE + WiFi on ESP32 requires modem sleep support to stay enabled.
  WiFi.setSleep(true);
  WiFi.disconnect(true, true);
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool hasCredentials() {
  return g_ssid.length() > 0;
}

String ipString() {
  if (!isConnected()) return "";
  return WiFi.localIP().toString();
}

void ensureConnected() {
  if (!hasCredentials()) {
    if (!g_missingCredsLogged) {
      Serial.println("WiFi skipped: credentials not configured");
      g_missingCredsLogged = true;
    }
    return;
  }

  wl_status_t status = WiFi.status();
  if (status == WL_CONNECTED) {
    if (g_lastStatus != WL_CONNECTED) {
      Serial.println("WiFi connected");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
    }
    g_attemptInProgress = false;
    g_lastStatus = status;
    return;
  }

  uint32_t now = millis();
  if (!g_attemptInProgress || (now - g_lastAttemptAt) >= WIFI_RETRY_INTERVAL_MS) {
    Serial.print("Connecting WiFi to SSID: ");
    Serial.println(g_ssid);
    WiFi.disconnect();
    WiFi.begin(g_ssid.c_str(), g_pass.c_str());
    g_lastAttemptAt = now;
    g_attemptInProgress = true;
  }

  if (status != g_lastStatus && status != WL_IDLE_STATUS) {
    Serial.print("WiFi status changed: ");
    Serial.println((int)status);
  }

  g_lastStatus = status;
}

} // namespace
