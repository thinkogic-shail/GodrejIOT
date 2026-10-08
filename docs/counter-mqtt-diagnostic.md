# Counter MQTT diagnostics

Subscribe to `godrej/counterdiagnostic/+` (or replace `+` with the UniqueCode).
Each completed UART attempt, including timeouts, updates a diagnostic snapshot.
Fields: UniqueCode, RequestType (`CounterDiagnostic`), AttemptSequence,
CapturedAtUptimeMs, CapturedDateTime (null until synchronized), Status,
ReceivedBytes, Valid, Partial, Error, and RawResponse.

Partial is true when bytes arrive but the read times out without a terminator.
A complete but invalid frame includes a parser error such as checksum_mismatch.
RawResponse contains the partial or complete capture, without its CR/LF
terminator; JSON escapes control characters. No-response timeouts contain an
empty RawResponse, ReceivedBytes=0, Valid=false, and Error=timeout.

Publishing needs working MQTT and a configured UniqueCode. The latest result
is retained only in RAM; new attempts overwrite older offline snapshots.
Failed publishes retry at most once per second. AttemptSequence resets at boot.
Messages are non-retained QoS 0, so subscribe before the next polling cycle.

Valid counters still publish to godrej/sendcounter/<UniqueCode>. Invalid and
partial data are never treated as trusted counters. BLE diagnostics consume
their own result flag independently. The serial-only counter_diagnostic build
disables MQTT; upload esp32dev for this test.
