# Cellular connectivity, time and recovery

## Implemented behavior

The existing WIFI/SIM mode selects the MQTT transport at boot. WIFI continues using WiFiClient. SIM uses the A7672S on UART2, RX GPIO16 / TX GPIO17 at 115200 8N1. Vending UART now defaults to dedicated UART1 RX GPIO13 / TX GPIO14; saved BLE pin overrides take precedence. Modem PWRKEY defaults to GPIO32 for the legacy Bharat Pi board; confirm this board revision before hardware testing. New Edgehax revisions use GPIO26. Change MODEM_POWER_KEY_PIN in config.h only if the board revision requires it.

Manufacturer reference: https://github.com/edgehax/4G-LTE-MODULE-SIMCOM-A7672S-ESP32/blob/main/4G-LTE-MODULE-SIMCOM-A7672S-ESP32.ino
AT reference: https://edgehax.com/wp-content/uploads/2023/12/A7672S_Series_AT_Command_Manual_V1.09.pdf

TinyGSM is pinned to 0.12.0. A local Client adapter handles plain TCP CIPSEND acknowledgements (upstream A7672X modemSend incorrectly waits for CCHSEND on the plain TCP path), checks confirmed byte counts, handles already-open NETOPEN on reconnect, and uses a 15-second TCP connection timeout. The installed library is not modified. This is the existing plain MQTT transport, not a TLS implementation.

SIM initialization probes AT before a 1-second PWRKEY pulse, waits between retries, checks SIM readiness and registration, activates PDP context 1, and checks data availability. PWRKEY retries are limited to once per 120 seconds. Network/PDP checks retry at 15-second intervals. SIM PIN is attempted at most once per boot to avoid repeated incorrect attempts. A missing SIM or unavailable operator does not erase configuration. No automatic Wi-Fi/SIM failover is implemented; mode selection remains explicit.

## Optional SIM settings

APN, APN username/password and SIM PIN default to blank. Blank APN preserves the modem/carrier PDP profile; it does not guarantee operator auto-provisioning, discover an APN, or clear a previously programmed modem APN. A carrier that requires an explicit APN needs configuration. Optional PAP credentials can be set for carriers requiring them. SIM secrets are stored in NVS like the existing Wi-Fi settings, not encrypted by this change.

Existing BLE RX supports:

```text
MAINTENANCE_ON
SET_SIM:apn|username|password|pin
SET_MODE:SIM
REBOOT
```

All four SET_SIM fields can be blank: SET_SIM:|||. This resets saved firmware overrides to blank (and disables PAP); an existing modem PDP APN is preserved. Changing settings requires maintenance and reboot. UPDATE_ALL retains its original fields; SIM settings are configured separately. To return to Wi-Fi, use SET_MODE:WIFI in maintenance then REBOOT.

GET reports cellularStatus and timeSynced. GET_SETTINGS / READ_ALL include simApn and boolean credential/PIN-configured flags, without returning new SIM passwords or PINs. AT delimiter/control characters are rejected. Existing BLE authentication limitations remain unchanged.

The board needs an active SIM, antenna and adequate external power for the modem. Manufacturer specifies 9V 2A input for cellular use; USB-only operation is not established by this build.

## Time synchronization

Wi-Fi: asynchronous SNTP against pool.ntp.org / time.nist.gov, with automatic stack resynchronization.
SIM: enables automatic timezone updates, attempts modem CNTP after data activation, and reads CCLK/NITZ. Retries once per minute while unsynchronized and every six hours after a plausible clock is acquired. Unsupported NTP or blocked servers fall back to the carrier clock. If neither source provides a plausible clock, timestamps stay null; no fixed fabricated date remains.

The ESP32 system clock stores UTC; formatted timestamps use Asia/Kolkata (fixed UTC+05:30). Modem timezone offsets are accounted for exactly once. Calendar fields are validated, including impossible dates, and clocks outside 2025-2099 are rejected. Plausibility validation cannot prove that a carrier's clock is accurate; compare it during commissioning.

StatusDateTime retains the previous dd/MM/yyyy, HH:mm:ss format when synchronized, otherwise JSON null. CounterDateTime uses yyyy-MM-dd HH:mm:ss and is captured with the reading, not at publish time. A reading acquired before synchronization retains a null timestamp. Server code must tolerate null. Device time is not retained across full power loss and is reacquired on boot.

## Recovery without erasing configuration

- Onboard RESET: directly resets the ESP32 enable line. Works without the application loop running; no firmware GPIO assignment is necessary. Preserves NVS settings.
- Power cycle: reboot while preserving NVS; use when hardware/power/modem faults cannot be recovered by ESP32 RESET alone.
- Optional extra GPIO button: remains disabled until a real GPIO is assigned. Both short and long presses now reboot, never factory reset. The onboard BOOT button is not repurposed.
- Main-loop task watchdog: 60-second timeout with panic/reset. Fed only by main-loop progress and completed network phases, not by an unrelated timer. It recovers a stalled firmware loop while preserving settings. It is not a network-outage reset policy.
- Factory reset remains the deliberate existing BLE RESET command. BLE REBOOT only restarts. Do not send RESET when you mean reboot.

Vending UART capture/polling now runs in a dedicated FreeRTOS task, so bounded modem AT/network waits do not prevent reading serial packets. Shared results are mutex-protected; the latest result/snapshot replaces older results in RAM during network waits. BLE notifications and MQTT publishing remain in the main loop and may be delayed while modem operations run. The worker does not write settings or call modem/MQTT APIs.

## Commissioning checks required

This change is compile-verified, not flashed or live-SIM tested here.

1. WIFI: confirm existing BLE/provisioning/counters/MQTT still work; verify SNTP and timestamp timezone.
2. SIM: select mode, reboot with adequate external power, confirm GET cellularStatus progresses to connected, and confirm actual MQTT receipt.
3. Leave APN blank first; supply the operator APN if data activation fails. Check missing/locked SIM and wrong APN diagnostics.
4. Verify modem NTP/carrier time against a known clock, including UTC/IST offset and null behavior without time.
5. Disconnect network while polling: verify complete UART captures and recovery, without a reboot loop.
6. Test fragmented/partial/invalid counter responses while the modem is connecting.
7. Verify onboard RESET and BLE REBOOT preserve UniqueCode, network, broker, SIM settings and machine provisioning.
8. Fault-inject a stalled application in a dedicated test build to verify watchdog reset; do not intentionally stall a deployed vending device.

Still deferred: MQTT TLS, BLE authorization, server-side counter subscription, full hardware regression/soak tests. These changes do not establish production readiness on their own.

Current implementation update: factory-reset BLE RESET now requires MAINTENANCE_ON. Watchdog monitors both the main loop and the vending worker; each task feeds its own progress independently, so a stalled serial worker is also recoverable.
