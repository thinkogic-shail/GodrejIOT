#pragma once
#include <stdint.h>

namespace AutoRinsePolicy {
enum class Action { None, Clear, Paused, AlreadyAttempted, Cooldown, Send };
constexpr uint32_t COOLDOWN_MS = 300000;
constexpr Action decide(bool valid, uint32_t level, uint32_t number,
                        bool enabled, bool latched, bool hasAttempted,
                        uint32_t elapsed) {
  return !valid ? Action::None :
      (level != 1 || number != 24) ? Action::Clear :
      !enabled ? Action::Paused :
      latched ? Action::AlreadyAttempted :
      (hasAttempted && elapsed < COOLDOWN_MS) ? Action::Cooldown : Action::Send;
}
// Compile-time regression checks run in every firmware build.
static_assert(decide(true, 1, 24, true, false, false, 0) == Action::Send, "Initial rinse");
static_assert(decide(false, 1, 24, true, false, false, 0) == Action::None, "Invalid frame");
static_assert(decide(false, 0, 0, true, true, true, COOLDOWN_MS) == Action::None, "Timeout cannot rearm");
static_assert(decide(true, 1, 24, false, false, false, 0) == Action::Paused, "Maintenance");
static_assert(decide(true, 1, 24, true, true, true, COOLDOWN_MS) == Action::AlreadyAttempted, "Persistent request");
static_assert(decide(true, 4, 1, true, true, true, 0) == Action::Clear, "Valid ready rearms");
static_assert(decide(true, 2, 24, true, false, false, 0) == Action::Clear, "Different level");
static_assert(decide(true, 1, 23, true, false, false, 0) == Action::Clear, "Different code");
static_assert(decide(true, 1, 24, true, false, true, COOLDOWN_MS - 1) == Action::Cooldown, "Cooldown boundary");
static_assert(decide(true, 1, 24, true, false, true, COOLDOWN_MS) == Action::Send, "New occurrence after cooldown");
}
