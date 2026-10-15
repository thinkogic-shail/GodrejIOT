#pragma once
#include <stdint.h>

namespace UartPollSchedule {
enum class Read { None, Counter, Status };
constexpr bool due(uint32_t now, uint32_t last, uint32_t interval) {
  return now - last >= interval;
}
// Counters take priority. A due status read stays due until the UART is free.
constexpr Read next(bool counterDue, bool statusDue) {
  return counterDue ? Read::Counter : statusDue ? Read::Status : Read::None;
}
static_assert(next(false, true) == Read::Status, "Status runs without counters");
static_assert(next(true, false) == Read::Counter, "Counters run without status");
static_assert(next(true, true) == Read::Counter, "Counters win simultaneous deadlines");
static_assert(next(false, false) == Read::None, "No premature polls");
static_assert(due(20000, 0, 20000) && !due(20000, 0, 60000), "Independent 20s/60s deadlines");
static_assert(due(20000, 0, 10000) && !due(20000, 0, 30000), "Runtime interval changes");
static_assert(due(999, UINT32_MAX, 1000), "Scheduling survives millis rollover");
}
