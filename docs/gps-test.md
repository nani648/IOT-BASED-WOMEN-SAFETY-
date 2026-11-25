# GP-02 GPS wiring and firmware

The GP-02 GPS receiver is active in the current ESP32-S3 firmware. Its carrier VCC uses the confirmed 5 V input; share ground with the ESP32. A 5 V supply input does not imply a 5 V UART signal. Confirm the carrier TX logic level or measure it safely before connecting it to an ESP32 input; add level shifting if needed.

| GP-02 pin | ESP32-S3 connection |
|---|---|
| VCC | Regulated 5 V supply |
| GND | Common ground |
| TX | Board RX pad / GPIO44 |
| RX | Board TX pad / GPIO43; optional for receive-only NMEA |
| PPS, N/F | Leave disconnected |

The GPS firmware uses UART1 at 9600 baud. Its most recent valid location is included in SOS SMS messages. The GP-02 antenna needs a clear sky view for a fix. GPS signal wiring and board UART pad availability are described in [the complete pin map](esp32-s3-pinout.md).

The OLED uses 3V3, SDA GPIO8, and SCL GPIO9. GPS, SIM800L and ESP32 grounds must be connected together even when their power supplies are separate.
