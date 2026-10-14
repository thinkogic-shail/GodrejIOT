#pragma once
#include <Arduino.h>
#include <Client.h>

namespace CellularManager {
void begin();
void ensureConnected();
bool isConnected();
Client& client();
void disconnect();
// Resume the current runtime configuration after maintenance; no NVS reload.
void resume();
const char* status();
}
