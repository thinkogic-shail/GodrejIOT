#pragma once
namespace VendingUartConfig {
inline bool valid(int rx, int tx) {
  if (rx == 3 && tx == 1) return true; // Existing USB-shared wiring.
  // Conservative board-specific list: no modem, flash, SD/SPI or strap pins.
  const auto available = [](int pin) {
    return pin == 13 || pin == 14 || pin == 21 || pin == 22 ||
           pin == 27 || pin == 33;
  };
  return rx != tx && available(rx) && available(tx);
}
}
