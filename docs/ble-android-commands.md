# Android BLE command contract

Removed duplicate aliases GET_SETTINGS and BYE. Use READ_ALL and DISCONNECT.
SET_NETWORK is unsupported: use SET_MODE and SET_WIFI/SET_SIM.
Vending UART is fixed at RX13/TX14 and has no BLE configuration commands.

Commands with no arguments:
GET, READ_ALL, READ_COUNTER_ONCE, MAINTENANCE_ON, REBOOT, DISCONNECT, RESET.
GET reads runtime status; READ_ALL reads saved settings.

Configuration syntax (literal pipe separators):
```
SET_UNIQUE_CODE:UNIQUE_CODE
SET_MODE:WIFI
SET_MODE:SIM
SET_WIFI:SSID|PASSWORD
SET_MQTT:HOST|PORT|USERNAME|PASSWORD
SET_SIM:APN|USERNAME|PASSWORD|PIN
UPDATE_ALL:UNIQUE_CODE|MODE|SSID|WIFI_PASSWORD|MQTT_HOST|MQTT_PORT|MQTT_USERNAME|MQTT_PASSWORD
```

Send MAINTENANCE_ON after connection setup and before configuration or RESET.
Wait for each command's JSON acknowledgement. DISCONNECT ends maintenance when
BLE disconnects; link loss also clears maintenance and resumes automatic operation.
REBOOT applies saved settings; disconnect resumes the previous runtime settings
until reboot. There is no MAINTENANCE_OFF command and no Android polling required.

SIM fields are optional; send all four fields with exactly three separators.
Empty fields clear saved values. READ_ALL includes simApn,
simCredentialsConfigured (username present), and simPinConfigured, without SIM
password/PIN values. Android should serialize one command from the whole SIM form.
Reject pipe delimiters inside SIM fields; PIN is empty or 4-8 digits.

UPDATE_ALL retains the existing eight-field format. It excludes SIM settings and
still requires SSID in SIM mode. Prefer individual commands for SIM configuration.

Subscribe to TX notifications before commands. READ_COUNTER_ONCE returns queued
or busy, then counter_read_finished. On completion, long-read characteristic
6E400004-B5A3-F393-E0A9-E50E24DCCA9E for JSON status/bytes/valid and, on success,
omittedSlot13 plus temporary/permanent arrays. Read 0005 for raw bytes. Arrays
have 15 entries: beverage slots 1-14 (14 hot water), then totals. A transport
complete frame may still have valid=false; validate that flag in Android.

BLE service: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E.
Write: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E.
Notify: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E.
All text responses are queued and fragmented to the negotiated MTU minus 3 bytes,
with 20-byte fallback and 512-byte cap. Fragments preserve UTF-8 character boundaries,
are paced 20 ms apart and sent as raw text without extra envelopes or separators.
Android accumulates fragments until a complete JSON object is parsed. Do not cancel
the application timeout on the first fragment. Clear partial buffers on disconnect
and new sessions. A single worker serializes whole responses so frames cannot
interleave. Old-session queued responses are discarded; failed fragment delivery
or queue exhaustion disconnects the link for a clean retry. No field renaming needed.

## BLE connection recovery and Android flow

Firmware checks disconnected advertising every 2 seconds and retries if stopped.
One technician connection is accepted. Setup must enable TX notifications and
send a nonempty command within 30 seconds. Commands before notification
subscription are ignored. Established sessions have no inactivity timeout and
need no heartbeat/polling. Release/reboot disables advertising recovery.
Disconnect callbacks and the disconnected recovery check clear maintenance.
Main-loop recovery resumes UART polling, Wi-Fi/cellular and MQTT using the current
runtime configuration. Saved settings take effect on reboot. Cellular resume
re-enables connection attempts without reinitializing UART or reloading SIM secrets.
Timeout boundary and millis rollover checks run at compile time.

Android flow:
1. Connect button scans and shows nearby devices; selection connects BLE.
2. Discover services, negotiate adequate MTU, enable TX notifications and write
   CCCD. Wait for each successful callback through a serialized GATT operation queue.
3. Send MAINTENANCE_ON and wait for its JSON ack before displaying Ready or
   enabling maintenance actions. No periodic GET/READ_ALL is required.
4. Read/update settings and counters only on user action, one command at a time.
5. Disconnect button sends DISCONNECT and always disconnects/closes BluetoothGatt
   after callback or bounded timeout. Cancel queued operations and clear stale refs.
6. Connection/setup/command failures clean up GATT similarly before retrying.
7. Lost link/app termination clears firmware maintenance once BLE detects link loss.
   If Android keeps a live connection open, firmware cannot infer abandonment;
   Android must close on exit/error. Established idle sessions are intentionally kept.
8. To apply saved configuration, send REBOOT and treat disconnection as expected.

Typical command-ack timeout: 5 seconds. Counter completion timeout: about 10 seconds.
A write callback alone is not the firmware acknowledgement. On a lost maintenance
ack, close/disconnect to let firmware clear the mode before retrying. There is no
need to add SET_NETWORK, GET_SETTINGS, BYE or UART configuration commands.

## MQTT diagnostics control

Send `DIAGNOSTICS_ON` or `DIAGNOSTICS_OFF` after subscribing to TX notifications.
These commands do not require maintenance mode and change runtime state immediately.
Acknowledgement includes `ok`, `msg`, and `intervalMs` (default 1000).
Diagnostics default to disabled after reboot. Disconnect BLE to resume MQTT if
maintenance mode was enabled; diagnostics remain enabled across BLE disconnect.
Subscribe to `godrej/diagnostics/<UniqueCode>` to receive the latest UART capture
each interval. Publishing does not change UART polling frequency.

## Reconnect to IoT

Android label: Reconnect to IoT. BLE command: RECONNECT_IOT (no parameters).
Requires maintenance mode and a saved nonempty UniqueCode. Invalidates/removes
only cached machine provisioning fields and clears their RAM snapshot. UniqueCode,
Wi-Fi, SIM, MQTT, network mode and schema are preserved. Cleanup failure returns
ok=false without scheduling a reboot; provisioning may already be invalidated.
Success response:
```
{"ok":true,"msg":"IoT reconnect scheduled. Rebooting"}
```
Restart is deferred to the main loop by 500 ms so the write callback returns and
the notification worker can send the acknowledgement. Android should show
Reconnecting, clear its BLE session on the expected disconnect, and avoid treating
that disconnect as an error. After reboot and MQTT connection, the normal startup
path subscribes to godrej/getinfo/<UniqueCode> and sends
{"MachineId":0,"UniqueCode":"TK100","RequestType":"Info-IOT"}
on godrej/sendinfo/<UniqueCode>. It saves a valid server response as usual.
The BLE success means cleanup/restart was scheduled, not that server registration
already completed. Existing server-response retry behavior is unchanged. A normal
REBOOT preserves cached machine information and does not force this refresh.
