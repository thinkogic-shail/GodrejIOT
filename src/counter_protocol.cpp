#include "counter_protocol.h"
#include <string.h>

namespace {
bool decimal(const char* start, const char* end, uint32_t& value) {
  if (start == end) return false;
  value = 0;
  for (const char* p = start; p != end; ++p) {
    if (*p < '0' || *p > '9') return false;
    uint32_t digit = *p - '0';
    if (value > (UINT32_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  return true;
}
int hexDigit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}
}

const char* CounterProtocol::parseMachineStatus(const char* frame, size_t length,
                                               uint32_t& level, uint32_t& number) {
  level = number = 0;
  if (!frame || length < 8 || length > 512) return "invalid_length";
  if (memcmp(frame, "*1E,", 4) != 0) return "invalid_header";
  const char* end = frame + length;
  const char* comma = nullptr;
  for (const char* p = frame; p < end; ++p) if (*p == ',') comma = p;
  if (!comma || end - comma < 2 || end - comma > 4) return "invalid_checksum_text";
  uint32_t expected = 0;
  for (const char* p = comma + 1; p < end; ++p) {
    int digit = hexDigit(*p);
    if (digit < 0) return "invalid_checksum_text";
    expected = expected * 16 + digit;
  }
  uint8_t checksum = 0;
  for (const char* p = frame; p <= comma; ++p) checksum ^= static_cast<uint8_t>(*p);
  if (expected != static_cast<uint32_t>(checksum) + 90) return "checksum_mismatch";
  const char* delimiter = frame + 4;
  while (delimiter < comma && *delimiter != ',') ++delimiter;
  uint32_t count;
  if (delimiter == comma || !decimal(frame + 4, delimiter, count) || count < 2)
    return "invalid_count";
  const char* start = delimiter + 1;
  uint32_t fields = 0, parsedLevel = 0, parsedNumber = 0;
  while (start <= comma) {
    delimiter = start;
    while (delimiter < comma && *delimiter != ',') ++delimiter;
    // Error Name can be empty (e.g. level 00 / number 00). Codes cannot.
    if (start == delimiter && fields < 2) return "invalid_structure";
    if (fields == 0 && !decimal(start, delimiter, parsedLevel)) return "invalid_status_level";
    if (fields == 1 && !decimal(start, delimiter, parsedNumber)) return "invalid_status_number";
    ++fields;
    if (delimiter == comma) break;
    start = delimiter + 1;
  }
  if (fields != count) return "field_count_mismatch";
  level = parsedLevel;
  number = parsedNumber;
  return nullptr;
}

const char* CounterProtocol::parse(const char* frame, size_t length, Reading& output) {
  output = Reading{};
  if (!frame || length > 512 || length < 8) return "invalid_length";
  if (memcmp(frame, "*1C,", 4) != 0) return "invalid_header";
  const char* end = frame + length;
  const char* checksumComma = nullptr;
  for (const char* p = frame; p != end; ++p) {
    if (*p == ',') checksumComma = p;
  }
  if (!checksumComma || checksumComma < frame + 4) return "invalid_structure";
  size_t hexLength = end - checksumComma - 1;
  if (hexLength < 1 || hexLength > 3) return "invalid_checksum_text";
  uint32_t receivedChecksum = 0;
  for (const char* p = checksumComma + 1; p != end; ++p) {
    int digit = hexDigit(*p);
    if (digit < 0) return "invalid_checksum_text";
    receivedChecksum = receivedChecksum * 16 + digit;
  }
  uint8_t checksum = 0;
  for (const char* p = frame; p <= checksumComma; ++p) checksum ^= static_cast<uint8_t>(*p);
  if (receivedChecksum != static_cast<uint32_t>(checksum) + 90) return "checksum_mismatch";

  const char* countEnd = frame + 4;
  while (countEnd < checksumComma && *countEnd != ',') ++countEnd;
  uint32_t count = 0;
  if (countEnd == checksumComma || !decimal(frame + 4, countEnd, count)) return "invalid_count";
  if (count != 28 && count != 30) return "unsupported_count";
  Reading parsed;
  parsed.receivedValues = count;
  parsed.omittedSlot13 = count == 28;
  const char* start = countEnd + 1;
  for (uint32_t i = 0; i < count; ++i) {
    const char* delimiter = start;
    while (delimiter < checksumComma && *delimiter != ',') ++delimiter;
    uint32_t value;
    if (!decimal(start, delimiter, value)) return "invalid_counter";
    size_t slot = i / 2;
    if (parsed.omittedSlot13 && slot >= 12) ++slot;
    if (i % 2 == 0) parsed.temporary[slot] = value;
    else parsed.permanent[slot] = value;
    if (i + 1 < count && delimiter == checksumComma) return "field_count_mismatch";
    if (i + 1 == count && delimiter != checksumComma) return "field_count_mismatch";
    start = delimiter + 1;
  }
  output = parsed;
  return nullptr;
}
