#pragma once
#include <stddef.h>
#include <stdint.h>

namespace BleTextFraming {
constexpr size_t payloadSize(uint16_t mtu) {
  return mtu < 23 ? 20 : (mtu > 515 ? 512 : mtu - 3);
}
constexpr size_t characterBoundary(const char* text, size_t offset, size_t end) {
  return end > offset && (static_cast<uint8_t>(text[end]) & 0xC0) == 0x80
      ? characterBoundary(text, offset, end - 1) : end;
}
// Android decodes each notification as UTF-8; keep complete characters.
constexpr size_t chunkSize(const char* text, size_t length, size_t offset, size_t capacity) {
  return offset >= length || capacity == 0 ? 0 :
      length - offset <= capacity ? length - offset :
      characterBoundary(text, offset, offset + capacity) - offset;
}
static_assert(payloadSize(23) == 20, "Default MTU");
static_assert(payloadSize(255) == 252, "Reported Android MTU");
static_assert(payloadSize(247) == 244, "Requested Android MTU");
static_assert(payloadSize(0) == 20, "Unknown MTU fallback");
static_assert(payloadSize(517) == 512, "Characteristic maximum");
static_assert(chunkSize("abcdef", 6, 0, 3) == 3, "ASCII first chunk");
static_assert(chunkSize("abcdef", 6, 3, 3) == 3, "Exact final chunk");
static_assert(chunkSize("abcdef", 6, 5, 3) == 1, "Short final chunk");
static_assert(chunkSize("abcdef", 6, 6, 3) == 0, "Complete frame");
static_assert(chunkSize("a\xE2\x82\xAC" "b", 5, 0, 3) == 1, "No split UTF-8 character");
static_assert(chunkSize("a\xE2\x82\xAC" "b", 5, 1, 3) == 3, "Full UTF-8 character");
}
