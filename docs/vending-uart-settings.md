# Configurable vending UART pins

Default: RX GPIO13 / TX GPIO14, dedicated UART1.
Legacy RX GPIO3 / TX GPIO1 remains available explicitly and shares USB UART0.
Saved alternate pins use dedicated UART1; UART2 remains reserved for the modem.
Baud is fixed at 9600 8N1, no flow control. Pin routing does not change voltage.

BLE commands (RX first, TX second):

```
GET_VENDING_UART
MAINTENANCE_ON
SET_VENDING_UART:13|14
GET_VENDING_UART
REBOOT
```

SET requires maintenance mode and saves the pair atomically in NVS. It does not
reroute a running UART. Power off before moving the Bharat Pi signal wires to
the saved GPIOs, then power on to apply. Machine-end wiring and GND stay unchanged.
Upload configuration-capable firmware before selecting or moving to new pins.

GET reports saved vendingRxPin/vendingTxPin, activeRxPin/activeTxPin, baudRate,
uartReady and rebootRequired. READ_ALL/GET_SETTINGS also expose saved pins.
MQTT diagnostics include active RxPin and TxPin. Android UI support is separate.

Allowed distinct alternate GPIOs: 13, 14, 21, 22, 27, 33. Use only pins unoccupied
by other peripherals on the actual board. Legacy pair 3|1 is also accepted.
The conservative list excludes modem, flash, SD/SPI, input-only and strap pins.
Invalid inputs leave saved settings unchanged. Missing/invalid stored settings
fall back to defaults. Normal reboot/upload preserves NVS; factory reset clears it.

To return to original wiring, save SET_VENDING_UART:3|1, power off, restore wires,
then power on. Both esp32dev and machine_status_probe use the same saved pins.

Saved BLE pin overrides take precedence over compiled defaults. Check GET_VENDING_UART
before moving wires. No pin override has been written to the test board during development.
