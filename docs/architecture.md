# Current architecture and behavior

## Standby and wake

After startup initialization, the OLED is off and the ESP32 enters light sleep between work. GPIO2 wakes the device and shows the status screen for 30 seconds. The screen shows GPS-derived IST time, network registration, GPS state/satellite count, and the most recent valid coordinates. The ESP32 can still be kept awake by an open native USB Serial Monitor for debugging.

## SOS flow

A debounced 2-second hold on GPIO1 activates SOS once per press. Firmware vibrates the motor, submits a location SMS to Guardian 1 when the modem is registered, and calls Guardian 1 after the SMS transaction completes. If SMS submission fails, it still attempts the call. If the call fails before becoming active, it retries once after the configured cooldown. During a call, a new 3-second hold on GPIO1 sends ATH. The same held press cannot activate another action until released.

The current configuration contains one real Guardian number. `GUARDIAN_2` is a placeholder and is intentionally not dialed or messaged. A recent GPS location is included in the SMS; if there is no valid fix, the SMS says the location is not available.

## Power boundary

The ESP32-S3 enters light sleep; the GP-02 and SIM800L supplies remain on. Their current draw is therefore not eliminated in standby. The modem/GPS UARTs are serviced continuously during SOS and after a button wake. The modem remains powered and can maintain or reacquire network registration while the ESP32 sleeps; the firmware checks its state after waking.

## GPIO map

| Function | GPIO |
|---|---:|
| OLED SDA / SCL | 8 / 9 |
| SIM800L TX / RX from ESP view | 11 / 12 |
| GPS ESP RX / TX | 44 / 43 (board RX / TX pads) |
| External wake button | 2 |
| SOS button | 1 |
| Vibration driver control | 10 |

GPIO5, GPIO6, GPIO7 and GPIO0 are unused. Confirm GPIO43/44 are exposed on the exact board revision. Wiring and motor driver details are in [esp32-s3-pinout.md](esp32-s3-pinout.md).

