#pragma once

#include <Arduino.h>
#include "counter_protocol.h"
#include <time.h>

namespace VendingSerial {
// Defaults to dedicated UART1 RX13/TX14; saved legacy RX3/TX1 uses UART0.
bool begin();
bool isReady();
int activeRxPin();
int activeTxPin();
// Thread-safe request queue; UART work runs in the dedicated worker task.
bool requestCounterRead();
void setPollingEnabled(bool enabled);
void enableWatchdog();
void loop();
// Main-loop consumers only. Failed reads invalidate the latest snapshot.
bool latestReading(CounterProtocol::Reading& output, uint32_t& capturedAt,
                   uint32_t* sequence = nullptr, time_t* capturedEpoch = nullptr);
uint32_t readingSequence();
// Raw capture only: complete means terminated, not protocol-validated.
bool takeResult(String& status, String& raw);
// Independent snapshot: does not consume the BLE result. Includes failed reads.
bool latestResult(String& status, String& raw, uint32_t& attempt,
                  uint32_t& finishedAt, time_t& finishedEpoch);
}
