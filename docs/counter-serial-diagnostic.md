# Temporary counter UART diagnostic

Build/upload the `counter_diagnostic` PlatformIO environment. The default
`esp32dev` environment remains the normal network/MQTT firmware.

```powershell
pio run -e counter_diagnostic -t upload --upload-port COM5
pio device monitor --port COM5 --baud 9600
```

Close any existing COM5 monitor before uploading. The diagnostic firmware
preserves NVS settings, skips Wi-Fi/cellular/MQTT startup, and polls the vending
machine every 10 seconds unless BLE maintenance mode pauses polling.

Each request is followed by `[COUNTER RX]`, `[COUNTER RAW]`, optional
`[COUNTER ERROR]`, and `[COUNTER END]` lines. `status=complete valid=true`
confirms a terminated response that passed protocol validation. `timeout
bytes=0` means no bytes were captured within two seconds; a nonzero byte
count with timeout means the frame did not terminate in time. Nonprintable
captured bytes are displayed as `\xNN`.

UART0 is shared with USB, so these diagnostic lines also reach the vending
machine RX. Its behavior on unsolicited diagnostic text is not verified.
Logs are printed only after capture finishes, before the next request; input
received between requests is drained before sending the next command.
This is a bench diagnostic, not a production logging configuration.

Restore normal behavior by uploading `esp32dev` again. No factory reset is needed.
