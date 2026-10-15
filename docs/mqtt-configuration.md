# MQTT configuration

After machine provisioning, each boot requests configuration on
`godrej/configuration/<UniqueCode>`:

```json
{"MachineId":89,"UniqueCode":"22A03P1","RequestType":"Configuration"}
```

MachineId and UniqueCode are the actual configured device values. The firmware
subscribes to `godrej/configuration-ack/<UniqueCode>` at QoS 1 alongside getinfo
on each MQTT connection before requesting configuration. The request is
non-retained QoS 0, attempted once per boot. Missing/invalid responses, publish
failure, and MQTT reconnects do not trigger retries. Normal operation continues
immediately with saved settings, or firmware defaults when none have been saved.
Every reboot allows one new request, even when settings were restored from NVS.

The server response is a complete object:

```json
{
  "ApkId": 0,
  "ApkVersion": "",
  "FilePath": "",
  "IsActive": "",
  "VideoDownloadStartTime": "22:00",
  "VideoDownloadStopTime": "06:00",
  "MQTTPushInterval": 20000,
  "MQTTStatisticsInterval": 3600000,
  "ScreensaverInactiveTimeout": 10000,
  "MQTTPushCounterInterval": 60000
}
```

All listed fields are required. ApkId must be a nonnegative 32-bit integer;
string fields must be strings (including IsActive). Times use 24-hour HH:mm.
Intervals/timeouts must be integer milliseconds from 1 through 2147483647.
Invalid JSON, missing fields, invalid values, or a failed NVS write leave active
settings unchanged without retrying. Replies received
before machine provisioning are ignored. Valid later replies also update settings.

All ten fields are retained in RAM and together in one NVS string. Unchanged
responses do not rewrite flash. Restart/power loss reloads the last saved settings;
factory reset removes them and restores firmware defaults. RECONNECT_IOT clears
machine provisioning only and preserves configuration.

Only these fields currently affect operation:

| Field | Publishing topic | Before first configuration |
| --- | --- | --- |
| MQTTPushInterval | godrej/sendstatus/<UniqueCode> | 5000 ms |
| MQTTPushCounterInterval | godrej/sendcounter/<UniqueCode> | 10000 ms |

The example sets status publishing to 20 seconds and counters to 60 seconds.
Changes apply immediately; intervals are measured against previous publish times.
Counters publish the latest valid, unpublished reading once the interval elapses;
the first fresh counter reading after startup can publish immediately. Failed
counter publishes retry at most once per second. Missing/invalid UART readings
can delay publishing; these settings do not change UART polling frequency.
The remaining configuration fields are stored for future use.

Diagnostics keep their separate 1000 ms interval and default to disabled on every
restart. Configuration does not enable diagnostics. Existing startup heartbeat
messages still publish independently on sendstatus.
