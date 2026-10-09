# Machine status probe

This environment now automatically sends rinse for validated level 1/code 24.
See auto-rinse.md for gating and diagnostics. It is no longer read-only.

Android MACHINE_STATUS is E,r,1,0 (no extra data argument). Its framed request
is `*1,E,r,1,0,5B\r`, using XOR plus decimal 90 and uppercase hexadecimal,
at 9600 8N1 without flow control.

Build/upload `machine_status_probe` to temporarily replace counter polling
with a status request every 10 seconds. Existing Wi-Fi/MQTT settings and the
counterdiagnostic topic are retained. Messages identify Command=MACHINE_STATUS,
RequestType=MachineStatusDiagnostic, transport Status, ReceivedBytes, raw
response and checksum validation. Valid replies also expose ErrorLevel,
ErrorNumber and MachineMessage mapped from the reference Android implementation.
Unknown codes remain visible with UNKNOWN STATUS. No counters are published
from these status frames. BLE counter validation is not a status decoder; use
the MQTT diagnostic and its Command field for this probe.

The parser expects *1E, data length, status level, status number, remaining data,
and checksum. Actual captured frames remain available if this structure differs
on a machine revision.

Exact request echoes are skipped while waiting for the actual response, including
echoes split across UART packets and echoes followed by a response in the same
packet. A temporary 5-second diagnostic deadline is not extended by echoes. Checksum validation is
unchanged. Diagnostics include `EchoCount`, `EchoBytes` (excluding terminators),
and `RawEcho`. `ReceivedBytes` and `RawResponse` describe only the response or
partial response after skipping echoes. A timeout with `EchoCount > 0` means
an echo was received but no complete machine response followed; a zero-byte
timeout with `EchoCount = 0` means neither was captured.

Upload esp32dev afterward to resume normal counter polling. Do not move wires
to TX2/RX2: GPIO17/16 are also connected to the cellular modem. A dedicated
vending UART requires verified free GPIOs, not those modem-connected pins.
