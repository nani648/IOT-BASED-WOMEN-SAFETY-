# Phase 1 — ESP32, Serial and OLED

## Wiring

Power off before wiring. For this phase use the development board's documented
USB power input only. Leave GPS, GSM, motor, Li-Po supply and SOS button disconnected.
Confirm the OLED accepts 3.3 V supply and has 3.3 V I2C pull-ups.

| OLED | ESP32-C3 board |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO4 |
| SCL | GPIO5 |

Use GPIO numbers, not connector position numbers. A four-pin SSD1306 needs no
separate reset wire. Set `OLED_ADDRESS` to the verified address, initially 0x3C.

## Complete source and build

All code needed for this phase is already in `platformio.ini`, `src/config.h`,
`src/display.h`, `src/display.cpp`, and `src/src.ino`. There are no missing
GSM/GPS stubs to fill in to build Phase 1.

1. Install VS Code and its PlatformIO IDE extension, or use an existing PlatformIO
   Core installation. Open `hardware_program` directly as the project.
2. Identify the board: verify ESP32-C3, flash capacity, and whether USB uses native
   GPIO18/19 or a USB-to-UART chip. Native USB is the default environment. For a
   bridge board, select `phase1_uart_console`; GPS must remain disconnected.
3. In a PlatformIO terminal, run the commands below. The first build downloads
   the pinned ESP32 platform, Arduino toolchain, and library dependencies.

```powershell
pio run -e phase1_native_usb
pio device list
pio run -e phase1_native_usb -t upload
pio device monitor -b 115200
```

If needed choose the actual COM port with `--upload-port COM7` on upload and
`--port COM7` on monitor; COM7 is only an example. Replace the environment name
in both build/upload commands for the UART console option. Press reset after
opening the monitor if you want to see initialization messages.

## Expected Serial Monitor

```text
[BOOT] ESP32 started
[PHASE 1] Serial + OLED test; SOS is not enabled
[OLED] SDA=4 SCL=5 address=0x3C
[OLED] Initialized SSD1306 128x64
[STATUS] Uptime=5 s OLED=OK
[STATUS] Uptime=10 s OLED=OK
```

Native USB can attach after OLED initialization, so that initialization line
may be missed. The boot announcement is deferred until a monitor connects, and
the five-second heartbeat reports current OLED availability. Startup never waits
indefinitely for a monitor.

## Expected OLED

```text
SAFETY WEARABLE
PHASE 1: OLED TEST
Time: --:--
GSM: NOT TESTED
GPS: NOT TESTED
Uptime: 10 s
SOS NOT ENABLED
```

Uptime advances approximately once per second. It is elapsed boot time, not local
clock time. A future GPS phase will provide actual time. There is no `READY`
claim because emergency communication is not implemented in this phase.

## Verification

1. Build must report SUCCESS, upload must complete, and the display must show
   the above seven lines without corruption.
2. Run for at least one minute. Verify advancing uptime and one Serial heartbeat
   every five seconds, with no repeated reset/boot logs.
3. Reset the board. Uptime restarts and the screen returns.
4. Close the monitor and reset: OLED must still work without a PC listener.
5. Power off, disconnect the OLED, then power on. Serial must continue heartbeats,
   report `OLED=UNAVAILABLE`, and report a retry every five seconds. Power off
   before reconnecting the OLED; reboot and verify normal output.
6. To test recovery without live wiring changes, temporarily configure a wrong
   address: firmware must continue running with UNAVAILABLE. Restore the verified
   address and rebuild. This verifies the missing-device path, not live recovery.

No SOS, SMS or voice-call acceptance is implied by passing Phase 1. Physical
tests above must be performed by the builder; they were not run during authoring.

## Common errors and fixes

| Symptom | Check / fix |
|---|---|
| `pio` not recognized | Use PlatformIO IDE's terminal or install PlatformIO Core |
| Upload cannot connect | Data-capable USB cable, correct port/driver; follow board BOOT/reset procedure |
| Native USB monitor empty | Confirm connector uses native USB; bridge boards need the UART-console environment |
| `No ACK` | Ground, SDA/SCL orientation, actual GPIO labels, supply, 0x3C versus 0x3D |
| ACK but blank/shifted image | Verify SSD1306 versus SH1106, 128×64 dimensions, reset requirement and power |
| Repeated resets | Supply/cable quality, shorts; keep other modules disconnected |
| Garbage Serial text on bridge | Monitor baud must match DEBUG_BAUD=115200 |
| Display intermittently fails | Shorter wires, correct pull-ups, common ground; retain 100 kHz while debugging |

`Adafruit_SSD1306::begin()` mainly initializes software/controller state; the
firmware separately checks an I2C acknowledgement. ACK does not prove the screen
is displaying correctly: visual inspection is still necessary. Driver APIs were
reviewed against the [Adafruit source](https://github.com/adafruit/Adafruit_SSD1306).

The loop has no `delay()` or wait-for-response loops. OLED transfers are still
synchronous (roughly 100 ms per frame at 100 kHz); each I2C transaction has a
timeout. This is adequate for Phase 1, but display rate/bus speed/UART buffer
budgets must be revisited when integrating modem and GPS traffic.
