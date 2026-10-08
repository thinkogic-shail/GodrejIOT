#pragma once
#include <stddef.h>
#include <stdint.h>

namespace CounterProtocol {
struct Reading {
  uint32_t temporary[15] = {};
  uint32_t permanent[15] = {};
  uint8_t receivedValues = 0;
  bool omittedSlot13 = false;
};
// Returns nullptr on success; output is cleared on any failure.
const char* parse(const char* frame, size_t length, Reading& output);
// Read-only E response: verify checksum/header/count and extract status codes.
const char* parseMachineStatus(const char* frame, size_t length,
                               uint32_t& level, uint32_t& number);
}
