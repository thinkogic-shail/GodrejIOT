#pragma once
#include <Arduino.h>

namespace ButtonManager {
  void begin(int pin, bool activeLow);
  void loop(); // call in main loop
}
