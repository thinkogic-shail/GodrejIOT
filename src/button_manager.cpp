#include "button_manager.h"
#include "storage.h"
#include "config.h" 

static int g_pin = -1;
static bool g_activeLow = true;

static bool g_last = false;
static uint32_t g_pressedAt = 0;
static bool g_handledLong = false;

static bool readPressed() {
  int v = digitalRead(g_pin);
  bool pressed = g_activeLow ? (v == LOW) : (v == HIGH);
  return pressed;
}

namespace ButtonManager {

void begin(int pin, bool activeLow) {
  g_pin = pin;
  g_activeLow = activeLow;

  pinMode(g_pin, g_activeLow ? INPUT_PULLUP : INPUT_PULLDOWN);

  g_last = readPressed();
  g_pressedAt = 0;
  g_handledLong = false;

  Serial.print("ButtonManager init on GPIO ");
  Serial.println(g_pin);
}

void loop() {
  if (g_pin < 0) return;

  bool pressed = readPressed();
  uint32_t now = millis();

  // edge: not pressed -> pressed
  if (pressed && !g_last) {
    g_pressedAt = now;
    g_handledLong = false;
  }

  // while pressed: check long press
  if (pressed && g_pressedAt > 0 && !g_handledLong) {
    if (now - g_pressedAt >= LONG_PRESS_MS) {
      g_handledLong = true;
      Serial.println("🔴 Long press detected → FACTORY RESET");
      Storage::factoryReset(true); // clears NVS + reboots
    }
  }

  // edge: pressed -> not pressed
  if (!pressed && g_last) {
    uint32_t held = (g_pressedAt > 0) ? (now - g_pressedAt) : 0;

    // short press action (only if long press not already handled)
    if (!g_handledLong && held >= SHORT_PRESS_MS) {
      Serial.println("🟡 Short press detected → REBOOT");
      delay(200);
      ESP.restart();
    }

    g_pressedAt = 0;
    g_handledLong = false;
  }

  g_last = pressed;
}

} // namespace
