# ESP32-S3 Super Mini wiring

Use GPIO numbers, not header positions. Confirm the RX/TX pads and other GPIO labels on your exact Super Mini revision. ESP32-S3 maps U0TXD to GPIO43 and U0RXD to GPIO44 ([Espressif ESP32-S3-MINI-1 datasheet](https://documentation.espressif.com/esp32-s3-mini-1_mini-1u_datasheet_en.pdf)).

| Device pin | ESP32-S3 connection |
|---|---|
| OLED VCC / GND | 3V3 / GND |
| OLED SDA / SCL | GPIO8 / GPIO9 |
| SIM800L TXD | GPIO12 (ESP UART RX) |
| SIM800L RXD | GPIO11 (ESP UART TX) |
| SIM800L power / GND | Carrier-specified supply / common GND |
| GP-02 TX | Board RX pad / GPIO44 |
| GP-02 RX | Board TX pad / GPIO43; optional for NMEA receive |
| GP-02 VCC / GND | Confirmed 5 V carrier input / common GND |
| GP-02 PPS, N/F | Leave disconnected |
| External wake button | GPIO2 to GND; `INPUT_PULLUP` |
| External SOS button | GPIO1 to GND; `INPUT_PULLUP` |
| Vibration driver input | GPIO10 through transistor/MOSFET driver |

Only GPIO1, 2, 8, 9, 10, 11 and 12 are used from the 1–13 group. GPIO5, GPIO6, GPIO7 and GPIO0 are unused. The GPS uses the board RX/TX pads at GPIO44/43. The SIM800L UART is separate on GPIO11/12.

All signal grounds, ESP32 ground, and external supply grounds must be common. SIM800L TX bursts need a supply sized for the exact carrier and adequate wiring. A 5 V power input does not imply 5 V UART tolerance; verify UART levels and use level conversion if needed.

## Vibration motor driver

The specified motor is a 3 V, approximately 90 mA coin motor (2.5–3.7 V operating range). Use a regulated 3.0 V motor supply with current headroom for startup. Do not connect it directly to GPIO10, a 5 V rail, or an unregulated single-cell Li-ion battery that can rise to 4.2 V. GPIO10 is only the control signal.

For this motor, use a 2N2222 NPN as a low-side switch:

- Motor positive to regulated 3.0 V.
- Motor negative to the 2N2222 collector; emitter to common GND.
- GPIO10 to the base through a 330 Ω resistor; add a 100 kΩ base-to-GND pulldown.
- Fit a flyback diode across the motor: striped cathode to motor positive, anode to motor negative/collector.
- Join motor supply negative, ESP32 GND and the GPS/SIM800L supply grounds.

The SOS firmware drives GPIO10 through three pulses: 250 ms on, 180 ms off, 250 ms on, 180 ms off, then 800 ms on. For a motor current above the NPN's rating, use a suitable logic-level N-channel driver. Do not use the removed AO3401A P-channel part for this low-side circuit.

## Button behavior

- Tap GPIO2 button: wake from light sleep and show the status/clock screen for 30 seconds.
- Hold GPIO1 SOS button for 2 seconds: vibrate, send SOS SMS with a Maps link if GPS has a current fix, and call Guardian 1. A failed SMS does not prevent the call.
- During a call, hold SOS for 3 seconds to request hang-up.
- Release the button between actions.

Only the ESP32 enters light sleep. The GPS and SIM800L remain powered. With the native USB Serial Monitor connected, the firmware keeps the ESP32 awake for debugging. In Arduino IDE select **ESP32S3 Dev Module** and enable **USB CDC On Boot**.

