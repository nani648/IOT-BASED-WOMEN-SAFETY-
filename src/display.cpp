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
  if (!acknowledges()) return false;
  if (!oled.begin(SSD1306_SWITCHCAPVCC, Config::OLED_ADDRESS, false, false)) return false;
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setTextWrap(false);
  return true;
}

void StatusDisplay::begin(uint32_t now) {
  Wire.begin(Config::OLED_SDA, Config::OLED_SCL, Config::I2C_CLOCK_HZ);
  Wire.setTimeOut(Config::I2C_TIMEOUT_MS);
  lastRetry_ = lastFrame_ = now;
  ready_ = initialize();
  if (ready_) draw(now);
}

void StatusDisplay::setPage(DisplayPage page) {
  if (page_ == page) return;
  page_ = page;
  if (awake_ && ready_) draw(millis());
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
    if (!ready_) { lastRetry_ = millis(); return; }
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

void StatusDisplay::update(uint32_t now, const char *gpsState,
                           const GsmStatus &gsm, const char *gsmModemText,
                           const char *gsmSimText, const char *gsmNetworkText,
                           const char *callText,
                           const GpsStatus &gpsStatus) {
  gpsState_ = gpsState; gsm_ = gsm; gsmModemText_ = gsmModemText;
  gsmSimText_ = gsmSimText; gsmNetworkText_ = gsmNetworkText;
  callText_ = callText; gpsStatus_ = gpsStatus;
  if (!awake_) return;
  if (!ready_) {
    if (uint32_t(now - lastRetry_) >= Config::DISPLAY_RETRY_MS) {
      lastRetry_ = now; ready_ = initialize();
      if (ready_) { draw(now); lastFrame_ = now; }
    }
    return;
  }
  if (uint32_t(now - lastFrame_) < Config::DISPLAY_REFRESH_MS) return;
  lastFrame_ = now;
  if (!acknowledges()) { ready_ = false; lastRetry_ = now; return; }
  draw(now);
}

void StatusDisplay::draw(uint32_t now) {
  oled.clearDisplay();
  if (page_ == DisplayPage::Clock) {
    uint32_t seconds = 0;
    const bool valid = gpsStatus_.clockValid;
    if (valid) {
      seconds = gpsStatus_.utcSecondsOfDay +
          uint32_t(now - gpsStatus_.lastClockUpdateTime) / 1000U +
          uint32_t(Config::TIMEZONE_OFFSET_MINUTES * 60);
      seconds %= 86400U;
    }
    char time[8];
    if (valid) snprintf(time, sizeof(time), "%02lu:%02lu",
        static_cast<unsigned long>(seconds / 3600U),
        static_cast<unsigned long>((seconds / 60U) % 60U));
    else snprintf(time, sizeof(time), "--:--");
    // Watch face: the only information on this page is time.
    oled.setTextSize(3);
    int16_t x1, y1; uint16_t w, h;
    oled.getTextBounds(time, 0, 0, &x1, &y1, &w, &h);
    oled.setCursor((128 - w) / 2, 13);
    oled.print(time);
    oled.setTextSize(2);
    char sec[3] = "--";
    if (valid) snprintf(sec, sizeof(sec), "%02lu", static_cast<unsigned long>(seconds % 60U));
    oled.setCursor(49, 48);
    oled.print(sec);
  } else if (page_ == DisplayPage::Sim) {
    oled.setTextSize(1); oled.setCursor(0, 0);
    oled.print("SIM800L");
    for (uint8_t i = 0; i < 4; ++i) oled.drawFastHLine(0, 11 + i, 128, SSD1306_WHITE);
    oled.setCursor(0, 15); oled.print("MODEM "); oled.print(gsmModemText_);
    oled.setCursor(0, 26); oled.print("SIM   "); oled.print(gsmSimText_);
    oled.setCursor(0, 37); oled.print("NET   "); oled.print(gsmNetworkText_);
    oled.setCursor(0, 48); oled.print("SIGNAL ");
    if (gsm_.signalKnown) {
      const int dbm = -113 + 2 * gsm_.signalRssi;
      oled.print(gsm_.signalRssi); oled.print("/31 "); oled.print(dbm); oled.print("dBm");
      const uint8_t bars = static_cast<uint8_t>((gsm_.signalRssi * 4 + 15) / 31);
      for (uint8_t i = 0; i < 4; ++i) {
        const int height = 2 + i * 2;
        const int x = 116 + i * 3;
        const int y = 62 - height;
        if (i < bars) oled.fillRect(x, y, 2, height, SSD1306_WHITE);
        else oled.drawRect(x, y, 2, height, SSD1306_WHITE);
      }
    } else oled.print("--");
    oled.setCursor(0, 58);
    if (emergencyActive_) { oled.print("SOS "); oled.print(emergencyText_); }
    else { oled.print("CALL "); oled.print(callText_); }
  } else {
    oled.setTextSize(1); oled.setCursor(0, 0);
    oled.print("GPS STATUS");
    for (uint8_t i = 0; i < 4; ++i) oled.drawFastHLine(0, 11 + i, 128, SSD1306_WHITE);
    oled.setCursor(0, 17); oled.print("FIX "); oled.print(gpsStatus_.gpsFixValid ? "VALID" : gpsState_);
    oled.setCursor(0, 28); oled.print("SATELLITES ");
    if (gpsStatus_.satellitesValid) oled.print(gpsStatus_.satellites); else oled.print("--");
    if (gpsStatus_.gpsFixValid) {
      oled.setCursor(0, 40); oled.print("LAT "); oled.print(gpsStatus_.latitude, 5);
      oled.setCursor(0, 52); oled.print("LON "); oled.print(gpsStatus_.longitude, 5);
    } else {
      oled.setCursor(0, 43); oled.print("Waiting for satellite fix");
    }
  }
  oled.display();
}
