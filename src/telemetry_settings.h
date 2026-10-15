#pragma once
#include <atomic>
#include <stdint.h>

namespace TelemetrySettings {
// Runtime values in milliseconds. Status/counter intervals load from saved configuration.
extern std::atomic<uint32_t> statusIntervalMs;
extern std::atomic<uint32_t> counterIntervalMs;
extern std::atomic<uint32_t> diagnosticIntervalMs;
extern std::atomic<bool> diagnosticsEnabled;
}
