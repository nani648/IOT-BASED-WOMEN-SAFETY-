#pragma once

#include <Arduino.h>
#include <TinyGPS++.h>

struct GpsStatus {
  double latitude = 0;
  double longitude = 0;
  bool gpsFixValid = false;  // A cached valid position less than 30 seconds old.
  uint32_t lastGPSFixTime = 0;
  bool receivingBytes = false;
  bool receivingNmea = false;
  bool satellitesValid = false;
  uint32_t satellites = 0;  // Satellites used in GGA, not total in view.
  bool clockValid = false;
  uint32_t utcSecondsOfDay = 0;
  uint32_t lastClockUpdateTime = 0;
  uint8_t day = 0;
  uint8_t month = 0;
  uint16_t year = 0;
};

class GpsReceiver {
 public:
  void begin();
  void update();
  const GpsStatus &status() const { return status_; }
  const char *stateText(uint32_t now) const;

 private:
  TinyGPSPlus parser_;
  GpsStatus status_;
  uint32_t lastByteTime_ = 0;
  uint32_t lastNmeaTime_ = 0;
  bool seenByte_ = false;
  bool seenNmea_ = false;
};
