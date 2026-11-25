# SIM800L use in the integrated firmware

SIM800L UART wiring: TXD to ESP32 GPIO12, RXD from ESP32 GPIO11. Power the exact carrier from its specified high-current supply and connect its ground to ESP32 ground. Verify UART voltage levels before wiring; the carrier's power input voltage does not establish UART tolerance.

In final operation, a 2-second SOS hold submits the SOS SMS to Guardian 1 when the modem is registered. It then attempts the voice call even if the SMS transaction fails, and retries one failed/no-answer call after the configured cooldown. A 3-second SOS hold during a call requests hang-up. Serial Monitor output at 115200 reports SOS and modem transactions; the old manual SMS1/CALL1 test commands are not part of this runtime flow.

`+CMGS` indicates that the modem accepted SMS submission, not that the recipient handset received it. The microphone connects to the SIM800L carrier's MIC pads, not to the ESP32. For all power and motor wiring, see [esp32-s3-pinout.md](esp32-s3-pinout.md).
