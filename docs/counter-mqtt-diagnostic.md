# Counter MQTT diagnostics

Subscribe to `godrej/diagnostics/+` (or replace `+` with the UniqueCode).
Publishing defaults to disabled on every boot. BLE `DIAGNOSTICS_ON` enables it;
`DIAGNOSTICS_OFF` disables it. The runtime `diagnosticIntervalMs` variable in
`telemetry_settings.cpp` defaults to 1000 ms. Enabled publishing sends the latest
UART snapshot each interval; repeated AttemptSequence values mean the same capture.
This interval controls MQTT publishing, not UART polling.
Each completed UART attempt, including timeouts, updates a diagnostic snapshot.
Fields: UniqueCode, RequestType (`CounterDiagnostic` or `MachineStatusDiagnostic`), AttemptSequence,
CapturedAtUptimeMs, CapturedDateTime (null until synchronized), Status,
ReceivedBytes, Valid, Partial, Error, and RawResponse.

Partial is true when bytes arrive but the read times out without a terminator.
A complete but invalid frame includes a parser error such as checksum_mismatch.
RawResponse contains the partial or complete capture, without its CR/LF
terminator; JSON escapes control characters. No-response timeouts contain an
empty RawResponse, ReceivedBytes=0, Valid=false, and Error=timeout.

Publishing needs working MQTT and a configured UniqueCode. The latest result
is retained only in RAM; new attempts overwrite older offline snapshots.
Failed publishes retry at the configured interval. AttemptSequence resets at boot.
Messages are non-retained QoS 0, so subscribe before the next polling cycle.

Valid counters still publish to godrej/sendcounter/<UniqueCode>. Invalid and
partial data are never treated as trusted counters. BLE diagnostics consume
their own result flag independently. The serial-only counter_diagnostic build
disables MQTT; upload esp32dev for this test.

## Periodic machine status

`godrej/sendstatus/<UniqueCode>` publishes every 5000 ms by default, controlled
by runtime variable `TelemetrySettings::statusIntervalMs`. MachineId comes from
cached provisioning; UniqueCode comes from device configuration. ErrorLevel and
ErrorNo are strings from the latest validated machine status; ErrorName uses the
machine message mapping. StatusDateTime uses `YYYY-MM-DD hh:mm AM/PM` in IST
(null before time synchronization). RequestType is `Status`; ButtonNo,
EmployeeCode, EmployeeName, CardNo, FacilityCode and IssueType are empty strings.
Status is skipped until a valid machine status exists, and after failed status
reads until a new valid response arrives. Counter captures do not replace it.
The latest valid state may repeat between UART status polls. Existing startup
BOOT_HEARTBEAT messages remain on the same topic. Server configuration now sets
status and counter publish intervals and persists them across reboot; see
[MQTT configuration](mqtt-configuration.md). Diagnostics still default to disabled
on every boot with a separate 1000 ms interval.
