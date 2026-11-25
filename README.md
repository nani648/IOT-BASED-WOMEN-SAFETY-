# Women safety wearable - integrated firmware

The ESP32-S3 firmware uses the GP-02 GPS, SSD1306 OLED, SIM800L, external wake and SOS buttons, and a vibration motor driver.

- At startup, GPS and modem communication initialize for about 20 seconds. The OLED remains off.
- In standby, the ESP32 enters light sleep and wakes on either button. Tap the external wake button to show GPS-derived Indian Standard Time, network state, GPS state, and coordinates for 30 seconds.
- Hold SOS for 2 seconds to start an emergency: the motor vibrates, an SOS SMS with a Maps link is sent to Guardian 1 when registered, then the firmware places a call. It attempts the call even if SMS submission fails and retries the call once after a failed/no-answer attempt.
- During the call, hold SOS for 3 seconds to request hang-up.

Open `src/src.ino` in Arduino IDE. Select **ESP32S3 Dev Module**, enable **USB CDC On Boot**, and install TinyGPSPlus, Adafruit SSD1306, and Adafruit GFX. The Serial Monitor is at 115200 baud and only reports SOS/modem transaction activity; the old `SMS1`/`CALL1` bench commands are not used in the final button flow.

Signal pins: OLED GPIO8/9, SIM800L UART GPIO11/12, GPS RX/TX board pads GPIO44/43, wake GPIO2, SOS GPIO1, vibration driver GPIO10. GPIO5/6/7 and GPIO0 are unused. See [all wiring details](docs/esp32-s3-pinout.md).

**Power behavior:** Only the ESP32 enters light sleep. GPS and SIM800L remain powered from their supplies, so this is not whole-system power-off and total battery draw will still include both modules. Add separate, safely designed module power control if the power budget requires them to turn off.

Power down before changing wires. SIM800L transmit bursts need a suitable external supply and common ground. Do not power the modem or vibration motor from an ESP32 GPIO.
