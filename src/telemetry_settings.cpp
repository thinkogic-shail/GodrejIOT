#include "telemetry_settings.h"

namespace TelemetrySettings {
std::atomic<uint32_t> statusIntervalMs{5000};
std::atomic<uint32_t> counterIntervalMs{10000};
std::atomic<uint32_t> diagnosticIntervalMs{1000};
std::atomic<bool> diagnosticsEnabled{false};
}
