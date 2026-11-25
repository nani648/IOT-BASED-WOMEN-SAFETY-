#pragma once

#include <Arduino.h>

enum class GsmStep : uint8_t { StartAt, WaitAt, StartSim, WaitSim, StartRegistration, WaitRegistration,
  StartSmsMode, WaitSmsMode, StartSmsNumber, WaitSmsPrompt, WaitSmsResult,
  WaitDial, WaitCallList, WaitHangup };

struct GsmStatus {
  bool modemResponding = false;
  bool simStatusKnown = false;
  bool simReady = false;
  bool simPinRequired = false;
  bool simQueryFailed = false;
  bool registrationKnown = false;
  bool registered = false;
  int registrationStatus = -1;
  uint32_t successfulAtTests = 0;
  bool smsBusy = false;
  bool smsAccepted = false;
  bool callActive = false;
  bool callBusy = false;
  char lastCommand[32] = "--";
  char lastResponse[32] = "--";
  char lastAction[24] = "IDLE";
};

class GsmModem {
 public:
  void begin(uint32_t now);
  void update(uint32_t now);
  const GsmStatus &status() const { return status_; }
  const char *modemText() const;
  const char *networkText() const;
  const char *simText() const;
  const char *callText() const;
  bool requestSms(const char *number, const char *message);
  bool requestCall(const char *number);
  void requestHangup();
  void recordAction(const char *action);

 private:
  void readModemLines(uint32_t now);
  void sendCommand(const char *command, uint32_t now);
  HardwareSerial *serial_ = nullptr;
  GsmStep step_ = GsmStep::StartAt;
  GsmStatus status_;
  uint32_t deadline_ = 0;
  uint32_t nextAttempt_ = 0;
  char line_[96]{};
  size_t lineLength_ = 0;
  char phone_[24]{};
  char smsBody_[120]{};
  bool callInProgress_ = false;
  bool gotCallEntry_ = false;
  bool gotSmsReference_ = false;
  uint32_t nextCallPoll_ = 0;
  uint8_t operation_ = 0;
  bool manualTransaction_ = false;
};

