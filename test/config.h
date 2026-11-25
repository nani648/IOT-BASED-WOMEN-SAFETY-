#pragma once

#include <Arduino.h>

namespace Config {
// ESP32-S3 Super Mini mapping; GPIO43/44 use board RX/TX pads for GPS UART.
constexpr int OLED_SDA = 8;
constexpr int OLED_SCL = 9;
constexpr int GSM_TX = 11;
constexpr int GSM_RX = 12;
constexpr int GPS_RX = 44;
constexpr int GPS_TX = 43;
constexpr int WAKE_BUTTON = 2;  // External wake/display button, active low.
constexpr int SOS_BUTTON = 1;   // External button to GND, active low.
constexpr int MOTOR_CONTROL = 10;

constexpr uint32_t DEBUG_BAUD = 115200;
constexpr uint32_t GSM_BAUD = 9600;  // Verify modem's saved/autobaud setting.
constexpr uint32_t GPS_BAUD = 9600;  // Verify GP-02-Kit documentation.
constexpr uint32_t GSM_RESPONSE_TIMEOUT_MS = 1500;
constexpr uint32_t GSM_RETRY_MS = 5000;
constexpr uint32_t GSM_REGISTER_POLL_MS = 10000;
constexpr uint32_t GPS_DATA_TIMEOUT_MS = 5000;
constexpr size_t GPS_RX_BUFFER_BYTES = 2048;
constexpr size_t GPS_BYTES_PER_UPDATE = 512;
constexpr char GUARDIAN_1[] = "+917397892966";
constexpr char GUARDIAN_2[] = "+91XXXXXXXXXX";
constexpr char SMS_TEST_TEXT[] = "SIM800L test message. Please confirm receipt.";

constexpr uint8_t OLED_ADDRESS = 0x3C;
constexpr int OLED_WIDTH = 128;
constexpr int OLED_HEIGHT = 64;
constexpr int OLED_RESET = -1;  // Four-pin module; no separate reset wire.
constexpr uint32_t I2C_CLOCK_HZ = 100000;
constexpr uint16_t I2C_TIMEOUT_MS = 20;
constexpr uint32_t DISPLAY_REFRESH_MS = 1000;
constexpr uint32_t DISPLAY_RETRY_MS = 5000;
constexpr uint32_t DEBUG_HEARTBEAT_MS = 5000;

constexpr uint32_t BUTTON_DEBOUNCE_MS = 40;
constexpr uint32_t SOS_HOLD_MS = 2000;  // Hold SOS to reduce accidental alerts.
constexpr uint32_t SCREEN_AWAKE_MS = 30000;
constexpr uint32_t END_CALL_HOLD_MS = 3000;
constexpr uint32_t GPS_FRESHNESS_MS = 30000;
constexpr uint32_t CALL_TIMEOUT_MS = 30000;
constexpr uint32_t CALL_RETRY_COOLDOWN_MS = 60000;
constexpr uint32_t CALL_POLL_MS = 2000;
constexpr uint32_t GSM_RECOVERY_MS = 10000;
constexpr uint32_t AT_TIMEOUT_MS = 3000;
constexpr uint32_t SMS_TIMEOUT_MS = 60000;
constexpr uint32_t VIBRATION_SHORT_MS = 250;
constexpr uint32_t VIBRATION_GAP_MS = 180;
constexpr uint32_t VIBRATION_LONG_MS = 800;
// Use a non-inverting low-side NPN/MOSFET driver: HIGH turns motor on.
constexpr uint8_t MOTOR_ON_LEVEL = HIGH;
constexpr uint8_t MOTOR_OFF_LEVEL = LOW;
constexpr int TIMEZONE_OFFSET_MINUTES = 330;
}  // namespace Config
