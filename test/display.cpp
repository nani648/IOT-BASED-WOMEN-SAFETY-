#include "display.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

namespace {
Adafruit_SSD1306 oled(Config::OLED_WIDTH, Config::OLED_HEIGHT, &Wire,
                      Config::OLED_RESET, Config::I2C_CLOCK_HZ,
                      Config::I2C_CLOCK_HZ);
}

bool StatusDisplay::acknowledges() {
  Wire.beginTransmission(Config::OLED_ADDRESS);
  return Wire.endTransmission() == 0;
}

bool StatusDisplay::initialize() {
  // begin() alone does not prove a physical display is connected.
  if (!acknowledges()) {
    return false;
  }
  // Keep the custom Wire pins. No reset delays for this four-pin module.
  if (!oled.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS, false, false)) {
    return false;
  }
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextWrap(false);
  return true;
}

void StatusDisplay::begin(uint32_t now) {
  Wire.begin(Config::OLED_SDA, Config::OLED_SCL, Config::I2C_CLOCK_HZ);
  Wire.setTimeOut(Config::I2C_TIMEOUT_MS);
  lastRetry_ = now;
  lastFrame_ = now;
  ready_ = initialize();
  if (ready_) draw(now);
}

void StatusDisplay::setAwake(bool awake) {
  const bool changed = awake_ != awake;
  awake_ = awake;
  if (!awake) {
    if (changed && ready_) oled.ssd1306_command(SSD1306_DISPLAYOFF);
    return;
  }

  if (!ready_ || !acknowledges()) {
    ready_ = initialize();
    if (!ready_) {
      lastRetry_ = millis();
      return;
    }
  }
  if (changed) oled.ssd1306_command(SSD1306_DISPLAYON);
  draw(millis());
  lastFrame_ = millis();
}

void StatusDisplay::setEmergencyState(bool active, const char *state) {
  const bool changed = emergencyActive_ != active || strcmp(emergencyText_, state) != 0;
  emergencyActive_ = active;
  emergencyText_ = state;
  if (changed && awake_ && ready_) draw(millis());
}

void StatusDisplay::update(uint32_t now, const GpsStatus &gps, const char *gpsState,
                           const GsmStatus &gsm, const char *gsmModemText,
                           const char *gsmNetworkText, const char *callText,
                           const GpsStatus &gpsStatus) {
  gps_ = gps;
  gpsState_ = gpsState;
  gsm_ = gsm;
  gsmModemText_ = gsmModemText;
  gsmNetworkText_ = gsmNetworkText;
  callText_ = callText;
  gpsStatus_ = gpsStatus;
  if (!awake_) return;
  // Unsigned subtraction also handles millis() rollover.
  if (!ready_) {
    if (uint32_t(now - lastRetry_) >= Config::DISPLAY_RETRY_MS) {
      lastRetry_ = now;
      ready_ = initialize();
      if (ready_) {
        draw(now);
        lastFrame_ = now;
      }
    }
    return;
  }
  if (uint32_t(now - lastFrame_) < Config::DISPLAY_REFRESH_MS) return;
  lastFrame_ = now;
  if (!acknowledges()) {
    ready_ = false;
    lastRetry_ = now;
    return;
  }
  draw(now);
}

void StatusDisplay::draw(uint32_t now) {
  oled.clearDisplay();
  oled.setCursor(0, 0);
  if (emergencyActive_) {
    oled.println("*** SOS ACTIVE ***");
    oled.print("State: "); oled.println(emergencyText_);
    oled.print("Network: "); oled.println(gsmNetworkText_);
    oled.print("SMS: ");
    if (gsm_.smsBusy) oled.println("SENDING");
    else if (gsm_.smsAccepted) oled.println("ACCEPTED");
    else oled.println("NOT ACCEPTED");
    oled.print("Call: "); oled.println(callText_);
    oled.print("GPS: "); oled.print(gpsState_);
    if (gpsStatus_.satellitesValid) { oled.print(" Sats:"); oled.println(gpsStatus_.satellites); }
    else oled.println();
    if (gpsStatus_.gpsFixValid) {
      oled.print("Lat:"); oled.println(gpsStatus_.latitude, 6);
      oled.print("Lon:"); oled.println(gpsStatus_.longitude, 6);
    } else {
      oled.println("Location unavailable");
      oled.println("Searching for GPS fix");
    }
  } else {
    oled.println("SAFETY DEVICE READY");
    if (gpsStatus_.clockValid) {
      uint32_t localSeconds = gpsStatus_.utcSecondsOfDay +
          uint32_t(now - gpsStatus_.lastClockUpdateTime) / 1000U +
          uint32_t(Config::TIMEZONE_OFFSET_MINUTES * 60);
      localSeconds %= 86400U;
      char clockText[24];
      snprintf(clockText, sizeof(clockText), "Time: %02lu:%02lu:%02lu IST",
               static_cast<unsigned long>(localSeconds / 3600U),
               static_cast<unsigned long>((localSeconds / 60U) % 60U),
               static_cast<unsigned long>(localSeconds % 60U));
      oled.println(clockText);
    } else {
      oled.println("Time: waiting for GPS");
    }
    oled.print("Network: "); oled.println(gsmNetworkText_);
    oled.print("GPS: "); oled.print(gpsState_);
    if (gpsStatus_.satellitesValid) { oled.print(" Sats:"); oled.println(gpsStatus_.satellites); }
    else oled.println();
    if (gpsStatus_.gpsFixValid) {
      oled.print("Lat:"); oled.println(gpsStatus_.latitude, 6);
      oled.print("Lon:"); oled.println(gpsStatus_.longitude, 6);
    } else {
      oled.println("Location: waiting");
      oled.println("Hold SOS for emergency");
    }
  }
  // A bounded synchronous I2C transfer, not an asynchronous display driver.
  // At 100 kHz a frame takes about 100 ms. GPS has a 2048-byte RX buffer
  // and is serviced before/after refresh. No waiting for a satellite fix.
  oled.display();
}

