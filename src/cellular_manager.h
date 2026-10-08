#pragma once
#include <Arduino.h>
#include <Client.h>

namespace CellularManager {
void begin();
void ensureConnected();
bool isConnected();
Client& client();
void disconnect();
const char* status();
}
