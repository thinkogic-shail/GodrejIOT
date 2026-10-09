# Automatic rinse

Reference Android `MachineStatus.java` handles level 1/code 24 ("please rinse")
by calling `VendingCommands(RINSE, "100")`. `Commands.java` builds the complete
request `*1,D,w,1,100,5E\r` using XOR plus decimal 90; D,w,1 alone is incomplete.

Bharat Pi acts on checksum-validated status level 1/code 24. Normal `esp32dev`
reads counters every 10 seconds and queues a status read 1.5 seconds after each
counter attempt. Status requests share the worker UART; they cannot overlap
counter requests. Status replies do not invalidate the latest counter snapshot
or replace a pending BLE counter result. `machine_status_probe` polls status
directly and now also supports auto-rinse; it is no longer read-only.

An eligible status sends one rinse command. Repeated 1/24 reports do not repeat
it. Only a valid different status rearms the latch; malformed frames, echoes,
timeouts, and bad checksums cannot trigger or rearm rinsing. A five-minute
minimum between attempts also prevents rapid repeats across status transitions.
The latch/cooldown are RAM state and reset on reboot. Maintenance pauses automatic
status polling and prevents rinsing. The serial worker waits three seconds after
a rinse attempt before its next read. Android treats dispense/rinse as having no
required reply; writing the command is not confirmation of execution or completion.

On `godrej/counterdiagnostic/<UniqueCode>`, status diagnostic messages include
`AutoRinseAction`: `none`, `sent`, `write_failed`, `already_attempted`, `cooldown`,
or `maintenance_paused`. Attempts include `RinseCommand`. Subsequent valid machine
status is how to observe whether the rinse request cleared. The existing counter
topic and payload are unchanged. Run `esp32dev` for counters plus auto-rinse.

Eligibility regression checks are compile-time assertions in `auto_rinse_policy.h`.
Real-machine execution still needs validation after uploading.
