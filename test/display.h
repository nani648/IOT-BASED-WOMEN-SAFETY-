#pragma once

#include <Arduino.h>
#include "gps.h"
#include "gsm.h"

class StatusDisplay {
 public:
  void begin(uint32_t now);
  void setAwake(bool awake);
  void setEmergencyState(bool active, const char *state);
  void update(uint32_t now, const GpsStatus &gps, const char *gpsState,
              const GsmStatus &gsm, const char *gsmModemText, const char *gsmNetworkText,
              const char *callText, const GpsStatus &gpsStatus);
  bool isReady() const { return ready_; }

 private:
  bool initialize();
  bool acknowledges();
  void draw(uint32_t now);
  bool ready_ = false;
  bool awake_ = true;
  bool emergencyActive_ = false;
  uint32_t lastRetry_ = 0;
  uint32_t lastFrame_ = 0;
  GpsStatus gps_;
  const char *gpsState_ = "STARTING";
  GsmStatus gsm_;
  const char *gsmModemText_ = "STARTING";
  const char *gsmNetworkText_ = "CHECKING";
  const char *callText_ = "IDLE";
  const char *emergencyText_ = "READY";
  GpsStatus gpsStatus_;
};

