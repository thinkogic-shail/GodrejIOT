> Current wiring default: UART1 RX GPIO13 / TX GPIO14 (see vending-uart-settings.md). The RX0/TX0 sections below describe the earlier diagnostic wiring.

# Vending counter serial protocol

Status: Step 1 reference review complete. No firmware or hardware test performed.
Reference: C:/Thinkogic/Projects/Godrej/GodrejFMEFM/app/src/main/java/

## Serial transport

- SerialComm.java: baud rate 9600, default Android device /dev/ttyS0, native open flags 0.
- Response wait: 2000 ms (actual constant; comments referring to 5 seconds are stale).
- Incoming data is accumulated across reads until CR or LF.
- Android strips CR/LF and trims the resulting string.
- Android caps sanitized responses at 512 characters by truncation. Firmware should reject overflow rather than parse truncated data.
- Android opens the serial port per command and closes it afterward. Firmware can keep the UART open.
- SerialPort.java delegates port configuration to libSerialPort.so. Native source is absent; data bits, parity, stop bits and flow control cannot be confirmed from the supplied Java code. 9600 8N1 without flow control is a candidate for the first hardware test, not a verified driver setting.

## Request

CommunicationConstants.java defines READ_ALL_BOTH_COUNTERS as C,r,2,3.
Call sites supply additional data 0.
Commands.java constructs ASCII frames as:

    *1,<command>,<data>,<checksum><CR>

Counter request, with CR shown as an escape:

    *1,C,r,2,3,0,75\r

Wire bytes (hex):

    2A 31 2C 43 2C 72 2C 32 2C 33 2C 30 2C 37 35 0D

Checksum construction:
1. XOR every character in *1,C,r,2,3,0, including the opening asterisk and trailing comma.
2. Add decimal 90 to the XOR result (no mask in Android).
3. Format uppercase hexadecimal without explicit zero padding.

For this request: XOR = 0x1B; 0x1B + 90 = 0x75; checksum text = 75.

## Response parsing

ControlSettings.parseCounterResponse and BackgroundCommandService.updateCounterData:

    <header0>,<header1>,<temp1>,<perm1>,...,<last value>,<trailing field>

The diagram is structural only; literal response header values and the trailing field meaning are not established by these parsers.

- Skip the first two comma-separated fields.
- Exclude the final field (presumed checksum; Android does not validate it).
- Accept exactly 30 or 28 numeric counter fields, i.e. 33 or 31 total fields.
- Values alternate temporary, permanent for each slot.
- Android parses values using signed Java Integer.parseInt; counter width, rollover and reset semantics are not documented here.
- With 30 values: fill all 15 slots.
- With 28 values: fill slots 1-12, insert zero temporary/permanent for slot 13, then fill slots 14-15.

DatabaseHelper.updateBeverageCounter clarifies downstream mapping:
- Slots 1-12: button temporary/permanent counts.
- Slot 13: skipped by the database update.
- Slots 14-15: totals. Preserve both pairs; exact business meanings require further evidence.

Android does not establish response checksum rules or validate response header identity. Do not claim checksum validation until confirmed using a real response or protocol specification.

## Polling interval evidence

CloudSettings.java contains commented code multiplying MACHINE_READ_COUNTER_INTERVAL by 60 * 1000, with a fallback of 1440. This suggests minutes (1440 minutes = one day), but the code is inactive and does not establish the current backend contract. Confirm before implementing the scheduler.

## First hardware test (next implementation stage)

Keep the confirmed yellow RX, green TX and black GND wiring. Verify Bharat Pi UART/GPIO mapping and prevent debug output on the machine UART. Send the exact request once, collect raw bytes through CR/LF with a 2-second timeout, and compare with Android. Capture a real response to confirm headers, trailing field and slot mapping. Machine signal voltage remains a hardware fact that software cannot determine.

## Source locations

- serial/CommunicationConstants.java: READ_ALL_BOTH_COUNTERS
- serial/Commands.java: SendCommands, CreateCommand, GetChecksum
- serial/SerialComm.java: initializeSerialComm, sendCommandData, processDataReceived, sanitizeResponse
- android_serialport_api/SerialPort.java: native open declaration
- ControlSettings.java: parseCounterResponse
- serial/BackgroundCommandService.java: updateCounterData
- util/DatabaseHelper.java: updateBeverageCounter
- CloudSettings.java: commented interval conversion

## Confirmed specification and sample (user supplied)

These details supersede the earlier open questions above:

- Baud 9600; 8 data bits; no parity; 1 stop bit; no flow control (9600 8N1).
- STX is ASCII *; ETX is carriage return (0x0D).
- Initial polling interval is 10 seconds (10000 ms), explicitly requested by the user. Keep configurable for later changes; do not use backend intervals for now.
- Response prefix combines start/address/command as *1C (no commas between these components).
- The next field is the data-length field; supplied sample declares 30 and contains exactly 30 numeric counter values.

Supplied response (append CR on the wire):

    *1C,30,00001,00001,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00000,00001,00001,00000,00000,00000,00000,00002,00002,00004,00004,b5

Verification: 33 comma-separated fields = prefix + length + 30 values + checksum.
Applying the request checksum algorithm to the response body through its final comma yields B5, matching b5 case-insensitively. Use case-insensitive hexadecimal checksum comparison. This sample establishes checksum compatibility for this response.

Decoded sample pairs (temporary, permanent):
- Slot 1: (1, 1)
- Slots 2-10: (0, 0)
- Slot 11: (1, 1)
- Slots 12-13: (0, 0)
- Slot 14: (2, 2)
- Slot 15: (4, 4)

The supplied response diagram includes an illustrative data field 40 and length 3, unlike the concrete all-counter sample. For READ_ALL_BOTH_COUNTERS, use the concrete sample and Android parser mapping; do not insert an extra 40 field. Exact business meanings of totals in slots 14-15 remain unspecified.

Planned validation: expected *1C prefix, declared numeric field count matching actual count, supported 28/30 count, numeric counters, valid checksum, CR termination, timeout and overflow handling. The 28-value variant is supported by Android but has not yet been supplied as a real response sample.

## Step 2 implementation: UART configuration

- Manufacturer pinout confirms RX0 = GPIO3 and TX0 = GPIO1: https://bharatpi.net/wp-content/uploads/2024/08/Bharat-Pi-4G-LTE-Pinout.pdf
- vending_serial.cpp initializes the existing UART0 object at 9600 8N1 with flow control disabled and a 1024-byte receive buffer. It sends no commands.
- Application Serial output is disabled through the existing silent proxy; a compile-time guard prevents enabling it while UART0 is reserved. Arduino core debug logging is disabled in PlatformIO.
- USB serial uses the same UART: a serial monitor is not an independent debug channel and must not send input during machine communication.
- ESP32 ROM boot output and bootloader/crash output are outside the application log proxy. Startup bytes can still reach the machine. Suppressing ROM output can require hardware/eFuse changes; none are made here. Runtime UART configuration alone does not guarantee a silent TX pin during reset or flashing.
- begin() returns initialization status; isReady() exposes it for the next counter-request stage.
- No upload or live communication test is part of this step.

## Step 3: single counter request (implemented, not hardware-tested)

No automatic request, periodic polling or MQTT counter publishing is enabled.

Test after flashing:
1. Connect a BLE client (e.g. an existing diagnostic BLE app) to THINK-<chip ID>.
2. Subscribe to TX notifications (UUID 6E400003-B5A3-F393-E0A9-E50E24DCCA9E).
3. Write ASCII READ_COUNTER_ONCE to RX (UUID 6E400002-B5A3-F393-E0A9-E50E24DCCA9E).
4. One request is queued; further requests return busy until that capture finishes and is consumed by the main loop.
5. After the counter_read_finished event (or after about 2 seconds), read status UUID 6E400004-B5A3-F393-E0A9-E50E24DCCA9E. It contains JSON status and captured byte count.
6. Read raw UUID 6E400005-B5A3-F393-E0A9-E50E24DCCA9E using a long GATT read for the complete capture. It excludes the CR/LF terminator. Separate raw/status attributes avoid JSON expansion exceeding GATT's 512-byte limit.

Statuses: idle, complete, timeout, overflow, write_failed, uart_not_ready.
Complete means a nonempty terminated frame was captured, not that its checksum or counters are valid. Timeout retains partial bytes for diagnosis. Overflow retains the first 512 bytes and reports failure. Results persist until the next completed read and are available after a BLE reconnect.

UART operations run in the main loop; the BLE callback only queues a request atomically. Reception processes at most 128 bytes per iteration. Existing MQTT connect operations can still block the overall loop for their configured timeout; use the existing MAINTENANCE_ON command for an isolated serial test if necessary (networking then stays paused until reboot). No new blocking receive wait or retries are introduced.

## Step 4: counter validation and parsing

counter_protocol.cpp now validates *1C, declared count (28 or 30), actual field count, numeric unsigned 32-bit values, and hexadecimal checksum (case insensitive). Overflow, signs, empty values, malformed fields and checksum failures are rejected. Failed parsing clears the output. Values above UINT32_MAX are rejected; machine rollover behavior remains unknown.

After READ_COUNTER_ONCE, BLE status characteristic 0004 now includes valid. On success it also includes temporary/permanent arrays (15 slots each) and omittedSlot13. On failure it includes error; transport failures have valid=false. Raw characteristic 0005 remains unchanged. A terminated capture can have status=complete and valid=false: completion is separate from protocol validity.

No periodic polling or counter MQTT publication is enabled yet. test/counter_protocol_test.cpp contains standalone C++11 regression cases for the supplied sample, omitted-slot mapping, checksum and structure failures, and numeric boundaries. Host execution requires a native C++ compiler; none was found in the checked locations on this workstation.

## Step 5: automatic polling every 10 seconds

- COUNTER_POLL_INTERVAL_MS in config.h is 10000 and is the only polling interval source for now.
- First automatic request waits 10 seconds after UART initialization. Subsequent requests are spaced from the actual request start time, including manually requested reads.
- One queued/active/result-pending request at a time; no overlapping commands or catch-up bursts.
- Failed reads are retried on the next scheduled poll (no immediate retry burst).
- Polling runs independently of Wi-Fi/MQTT connectivity and UniqueCode provisioning.
- MAINTENANCE_ON disables new automatic requests; an in-flight request finishes normally. Explicit READ_COUNTER_ONCE remains available in maintenance. Existing maintenance ends on reboot.
- Latest validated counters and monotonic capture time are held only in RAM, exposed to main-loop consumers by latestReading(). Failed captures invalidate that snapshot so stale values cannot look like a new valid reading.
- BLE 0004/0005 update after each automatic or manual capture. Periodic polling does not publish counters over MQTT yet.
- Scheduler is millis-wrap-safe, runs cooperatively, and retains existing main-loop timing limitations during Wi-Fi/MQTT reconnect calls. Ten seconds is the target interval, not a hard real-time guarantee.
- This update is build-verified only; no new firmware upload or live counter test performed.

## Step 6: MQTT counter publishing

Device publishes non-retained QoS0 messages on godrej/sendcounter/<UniqueCode>, using the existing MQTT client. Server should subscribe to godrej/sendcounter/+. Device does not subscribe to its own counter topic.

Payload contract (example values from supplied response):

```json
{
  "MachineId": 123,
  "UniqueCode": "example-device",
  "RequestType": "Counter",
  "TempCount": "1,0,0,0,0,0,0,0,0,0,1,0,0,2,4",
  "PermanentCount": "1,0,0,0,0,0,0,0,0,0,1,0,0,2,4",
  "OmittedSlot13": false,
  "CounterSequence": 1,
  "CapturedAtUptimeMs": 10000,
  "CounterDateTime": null
}
```

TempCount/PermanentCount are CSV strings with exactly 15 slots, matching Android model naming. Slots 1-12 represent buttons; slot 13 is preserved or zero-filled and explicitly flagged if omitted; slots 14-15 are totals. MachineId is 0 if backend machine provisioning is not yet available. UniqueCode remains required for MQTT connection.

Only validated snapshots are published, once per successful local publish. Failed publish attempts are throttled to one second and retried while that snapshot remains valid; newer readings replace it and invalid readings invalidate it. Counters are not queued or stored to flash.

On each MQTT connection, the current sequence is skipped and a fresh read is requested (or an existing in-flight request completes after reconnect). Offline snapshots are not replayed. The first fresh reconnect read may occur sooner than the normal 10-second schedule. Periodic reads thereafter remain 10 seconds from the actual request start.

CounterSequence resets on reboot and is diagnostic, not a globally unique message identifier. CapturedAtUptimeMs is monotonic device uptime, not a calendar timestamp. CounterDateTime is null until real time synchronization exists; the server must record receipt time. Absolute counters can repeat across polls and must not be summed by the server.

PubSubClient's publish success means a successful local write, not a server acknowledgment. Current QoS0 delivery can lose messages; subsequent absolute readings reconcile the latest totals, but do not reconstruct lost intermediate history. No guarantee of counter-reset history is provided.

Build verified; no upload or end-to-end machine/broker test performed for this step. Server-side subscription/processing remains a separate change.

## Connectivity/recovery follow-up

See connectivity-time-recovery.md for the current SIM/Wi-Fi transport, actual clock synchronization, non-destructive button behavior and watchdog. Counter reception/polling now runs in a dedicated mutex-protected worker, superseding the earlier cooperative main-loop timing notes. CounterDateTime is the synchronized IST capture time when available, otherwise null. Main-loop MQTT/BLE delivery can still be delayed during bounded modem operations; only the latest RAM snapshot is retained.
