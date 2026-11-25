# Project status and next checks

The integrated firmware now supports ESP32 light-sleep standby, GPIO2 wake/display, GPS-derived IST clock, GPIO1 SOS activation, vibration feedback, an SOS SMS with current location when available, a call to Guardian 1, one call retry, and a 3-second hold to request hang-up.

## Bench sequence after upload

1. With USB Serial Monitor closed, confirm that the OLED stays off after startup and the ESP32 wakes when GPIO2 is pressed.
2. Tap GPIO2 and confirm the screen shows time/network/GPS status for about 30 seconds, then goes off.
3. Verify the modem has registered and GPS has a recent fix before a controlled SOS test.
4. Hold GPIO1 for 2 seconds and check vibration, received location SMS, and call to the configured consenting Guardian 1 phone.
5. During the call, hold GPIO1 for 3 seconds and confirm the call ends.
6. Repeat the SOS test with the GPS antenna view blocked or modem unregistered only in a controlled bench setup; confirm the OLED shows unavailable/waiting status.

Only the ESP32 enters light sleep. GPS and SIM800L remain externally powered, so measure whole-system battery draw before relying on battery life. The firmware has one configured real contact; the Guardian 2 value is still a placeholder and is not used.

Use the wiring and electrical notes in [esp32-s3-pinout.md](esp32-s3-pinout.md). The Arduino CLI build could not be run in this environment because it could not access the installed Arduino package directory; compile and upload from Arduino IDE with ESP32 core 3.2.0, **ESP32S3 Dev Module**, and **USB CDC On Boot** enabled.
