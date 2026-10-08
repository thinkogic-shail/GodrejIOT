// Host test: compile with src/counter_protocol.cpp using a C++11 compiler.
#include "../src/counter_protocol.h"
#include "../src/vending_uart_config.h"
#include <assert.h>
#include <stdio.h>
#include <string>

static std::string frame(const std::string& body) {
  unsigned char checksum = 0;
  for (char c : body) checksum ^= static_cast<unsigned char>(c);
  char hex[8];
  snprintf(hex, sizeof(hex), "%X", checksum + 90);
  return body + hex;
}
static std::string counters(int count, const char* first = "0") {
  std::string body = "*1C," + std::to_string(count) + ",";
  for (int i = 0; i < count; ++i) body += std::string(i == 0 ? first : "0") + ",";
  return body;
}
static const char* parse(const std::string& input, CounterProtocol::Reading& result) {
  return CounterProtocol::parse(input.data(), input.size(), result);
}
int main() {
  assert(VendingUartConfig::valid(3, 1));
  assert(VendingUartConfig::valid(13, 14));
  assert(VendingUartConfig::valid(14, 13));
  assert(!VendingUartConfig::valid(13, 13));
  assert(!VendingUartConfig::valid(16, 17));
  assert(!VendingUartConfig::valid(6, 7));
  assert(!VendingUartConfig::valid(0, 12));
  assert(!VendingUartConfig::valid(34, 35));
  assert(!VendingUartConfig::valid(3, 14));
  CounterProtocol::Reading result;
  const std::string sample = "*1C,30,00001,00001,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00001,00001,00000,00000,00000,00000,00002,00002,00004,00004,b5";
  assert(!parse(sample, result));
  assert(result.temporary[0] == 1 && result.permanent[10] == 1);
  assert(result.temporary[13] == 2 && result.permanent[14] == 4);
  assert(!result.omittedSlot13);
  std::string body = "*1C,28,";
  for (int i = 1; i <= 28; ++i) body += std::to_string(i) + ",";
  assert(!parse(frame(body), result));
  assert(result.omittedSlot13 && result.temporary[12] == 0 && result.permanent[12] == 0);
  assert(result.temporary[13] == 25 && result.permanent[14] == 28);
  assert(!parse(frame(counters(30, "4294967295")), result));
  assert(result.temporary[0] == UINT32_MAX);
  assert(parse(frame(counters(30, "4294967296")), result));
  assert(result.receivedValues == 0 && result.temporary[0] == 0);
  assert(parse(frame(counters(30, "-1")), result));
  assert(parse(frame(counters(30, "")), result));
  assert(parse(frame(counters(30, "1x")), result));
  assert(parse(frame(counters(29)), result));
  body = counters(28); body.replace(4, 2, "30");
  assert(parse(frame(body), result));
  body = counters(30); body.replace(4, 2, "28");
  assert(parse(frame(body), result));
  std::string invalid = sample; invalid[0] = '!';
  assert(parse(invalid, result));
  invalid = sample; invalid[7] = '2';
  assert(parse(invalid, result));
  assert(parse(sample.substr(0, sample.size() - 1), result));
  assert(parse(sample + ",", result));
  assert(parse(std::string(513, '0'), result));
  assert(CounterProtocol::parse(nullptr, 0, result));
  uint32_t level, number;
  std::string status = frame("*1E,3,4,1,0,");
  assert(!CounterProtocol::parseMachineStatus(status.data(), status.size(), level, number));
  assert(level == 4 && number == 1);
  std::string emptyName = frame("*1E,3,00,00,,");
  assert(!CounterProtocol::parseMachineStatus(emptyName.data(), emptyName.size(), level, number));
  assert(level == 0 && number == 0);
  status[status.size() - 1] = status.back() == '0' ? '1' : '0';
  assert(CounterProtocol::parseMachineStatus(status.data(), status.size(), level, number));
  status = frame("*1E,3,4,1,");
  assert(CounterProtocol::parseMachineStatus(status.data(), status.size(), level, number));
  status = frame("*1E,3,r,1,0,");
  assert(CounterProtocol::parseMachineStatus(status.data(), status.size(), level, number));
  assert(CounterProtocol::parseMachineStatus(nullptr, 0, level, number));
  puts("Counter protocol tests passed");
}
