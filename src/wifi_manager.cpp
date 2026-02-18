#include "wifi_manager.h"
#include <WiFi.h>

static const char* g_ssid = nullptr;
static const char* g_pass = nullptr;

namespace WiFiManager {

void begin(const char* ssid, const char* pass) {
  g_ssid = ssid;
  g_pass = pass;

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(200);
}

bool isConnected() {
  return WiFi.status() == WL_CONNECTED;
}

String ipString() {
  if (!isConnected()) return "";
  return WiFi.localIP().toString();
}

void ensureConnected() {
  if (isConnected()) return;

  Serial.println("\nConnecting WiFi...");
  WiFi.begin(g_ssid, g_pass);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 30000) {
    delay(500);
    Serial.print(".");
  }

  if (isConnected()) {
    Serial.println("\nWiFi connected ✅");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi FAILED ❌");
  }
}

} // namespace
