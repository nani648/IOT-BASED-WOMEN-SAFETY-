#include "gps.h"

#include <HardwareSerial.h>
#include "config.h"

namespace {
HardwareSerial gpsSerial(1);
}

// Native USB is used for debug. UART1 is dedicated to GPS and UART2 to GSM.
// GPIO19/20 are reserved for native USB on the common S3 Super Mini layout.
#if !defined(ARDUINO_USB_CDC_ON_BOOT) || !ARDUINO_USB_CDC_ON_BOOT
#error "Select ESP32S3 Dev Module and enable Tools > USB CDC On Boot."
#endif
#if !defined(CONFIG_IDF_TARGET_ESP32S3)
#error "This project requires an ESP32-S3 board selection."
#endif

void GpsReceiver::begin() {
  gpsSerial.setRxBufferSize(Config::GPS_RX_BUFFER_BYTES);
  // RX receives NMEA from GPS TX. GPS RX is optional and may stay disconnected.
  gpsSerial.begin(Config::GPS_BAUD, SERIAL_8N1, Config::GPS_RX, Config::GPS_TX);
}

void GpsReceiver::update() {
  // Bound each pass even if the input is noisy or continuously busy.
  size_t count = 0;
  while (gpsSerial.available() && count < Config::GPS_BYTES_PER_UPDATE) {
    const char incoming = static_cast<char>(gpsSerial.read());
    ++count;
    ++status_.bytesReceived;
    lastByteTime_ = millis();
    seenByte_ = true;
    if (parser_.encode(incoming)) {
      seenNmea_ = true;
      lastNmeaTime_ = millis();
    }
  }
  const uint32_t now = millis();
  if (parser_.location.isValid() && parser_.location.age() < Config::GPS_FRESHNESS_MS) {
    status_.latitude = parser_.location.lat();
    status_.longitude = parser_.location.lng();
    status_.lastGPSFixTime = now - parser_.location.age();
    status_.gpsFixValid = true;
  } else {
    status_.gpsFixValid = false;
  }
  if (parser_.time.isValid() && parser_.date.isValid() &&
      parser_.time.age() < Config::GPS_DATA_TIMEOUT_MS) {
    status_.utcSecondsOfDay = static_cast<uint32_t>(parser_.time.hour()) * 3600U +
        static_cast<uint32_t>(parser_.time.minute()) * 60U + parser_.time.second();
    status_.day = static_cast<uint8_t>(parser_.date.day());
    status_.month = static_cast<uint8_t>(parser_.date.month());
    status_.year = static_cast<uint16_t>(parser_.date.year());
    status_.lastClockUpdateTime = now - parser_.time.age();
    status_.clockValid = true;
  }
  status_.receivingBytes = seenByte_ &&
      uint32_t(now - lastByteTime_) < Config::GPS_DATA_TIMEOUT_MS;
  status_.receivingNmea = seenNmea_ &&
      uint32_t(now - lastNmeaTime_) < Config::GPS_DATA_TIMEOUT_MS;
  // Expire cached coordinates using TinyGPSPlus's age, including across light sleep.
  if (status_.gpsFixValid &&
      uint32_t(now - status_.lastGPSFixTime) >= Config::GPS_FRESHNESS_MS) {
    status_.gpsFixValid = false;
  }
  status_.satellitesValid = parser_.satellites.isValid() &&
      parser_.satellites.age() < Config::GPS_DATA_TIMEOUT_MS;
  if (status_.satellitesValid) status_.satellites = parser_.satellites.value();
  status_.goodSentences = parser_.passedChecksum();
  status_.badChecksums = parser_.failedChecksum();
}

const char *GpsReceiver::stateText(uint32_t now) const {
  if (!status_.receivingBytes) return "NO DATA";
  if (!status_.receivingNmea) return "CHECK BAUD";
  if (!status_.gpsFixValid) return "SEARCH";
  if (uint32_t(now - status_.lastGPSFixTime) >= Config::GPS_DATA_TIMEOUT_MS)
    return "CACHED FIX";
  return "FIX";
}
