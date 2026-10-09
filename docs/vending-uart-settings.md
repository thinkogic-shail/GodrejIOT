# Fixed vending UART

Vending serial uses dedicated hardware UART1 at 9600 8N1 without flow control.
Yellow connects to GPIO13 (RX), green to GPIO14 (TX), and black to GND.

UART pin configuration has been removed. GET_VENDING_UART and SET_VENDING_UART
are unsupported, and GET_SETTINGS/READ_ALL no longer contain vendingRxPin or
vendingTxPin. Any old vend_uart NVS value is ignored. No factory reset is needed;
Wi-Fi, SIM, MQTT, unique code and other saved settings remain available.

MQTT diagnostics still report RxPin=13 and TxPin=14 for verification.
