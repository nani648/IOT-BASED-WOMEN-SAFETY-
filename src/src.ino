#include <Arduino.h>
#include <esp_sleep.h>
#include <driver/gpio.h>
#include <string.h>
#include "config.h"
#include "display.h"
#include "gps.h"
#include "gsm.h"

namespace {
StatusDisplay display;
GpsReceiver gps;
GsmModem gsm;

struct ButtonState {
  bool rawHigh = true;
  bool stableHigh = true;
  bool longActionSent = false;
  uint32_t rawChangedAt = 0;
  uint32_t pressedAt = 0;
};
ButtonState wakeButton;
ButtonState sosButton;

bool emergencyActive = false;
bool smsQueued = false;
bool smsInFlight = false;
bool smsFinished = false;
bool callPending = false;
bool callRequested = false;
bool callObservedBusy = false;
bool callWasActive = false;
bool hangupRequestedByUser = false;
uint8_t callAttempts = 0;
uint32_t callRequestAt = 0;
uint32_t retryCallAt = 0;
uint32_t screenAwakeUntil = 0;
uint32_t startupRunUntil = 0;
bool vibrationActive = false;
uint8_t vibrationPhase = 0;
uint32_t vibrationDeadline = 0;
const char *emergencyText = "READY";
DisplayPage currentPage = DisplayPage::Clock;

void showNextPage(uint32_t now) {
  if (!display.isAwake() || !screenAwakeUntil) {
    currentPage = DisplayPage::Clock;
    display.setPage(DisplayPage::Clock);
    display.setAwake(true);
  } else {
    switch (currentPage) {
      case DisplayPage::Clock: currentPage = DisplayPage::Sim; break;
      case DisplayPage::Sim: currentPage = DisplayPage::Gps; break;
      case DisplayPage::Gps: currentPage = DisplayPage::Clock; break;
    }
    display.setPage(currentPage);
  }
  screenAwakeUntil = now + Config::SCREEN_AWAKE_MS;
}

bool updateButton(ButtonState &button, int pin, uint32_t now, bool &pressedEdge) {
  pressedEdge = false;
  const bool raw = digitalRead(pin) == HIGH;
  if (raw != button.rawHigh) {
    button.rawHigh = raw;
    button.rawChangedAt = now;
  }
  if (raw != button.stableHigh && uint32_t(now - button.rawChangedAt) >= Config::BUTTON_DEBOUNCE_MS) {
    button.stableHigh = raw;
    if (!raw) {
      button.pressedAt = now;
      button.longActionSent = false;
      pressedEdge = true;
    }
  }
  return !button.stableHigh;
}

void startVibration(uint32_t now) {
  vibrationActive = true;
  vibrationPhase = 0;
  digitalWrite(Config::MOTOR_CONTROL, Config::MOTOR_ON_LEVEL);
  vibrationDeadline = now + Config::VIBRATION_SHORT_MS;
}

void updateVibration(uint32_t now) {
  if (!vibrationActive || static_cast<int32_t>(now - vibrationDeadline) < 0) return;
  static constexpr uint16_t durations[] = {Config::VIBRATION_GAP_MS,
      Config::VIBRATION_SHORT_MS, Config::VIBRATION_GAP_MS,
      Config::VIBRATION_LONG_MS};
  ++vibrationPhase;
  if (vibrationPhase >= 5) {
    digitalWrite(Config::MOTOR_CONTROL, Config::MOTOR_OFF_LEVEL);
    vibrationActive = false;
    return;
  }
  digitalWrite(Config::MOTOR_CONTROL,
      (vibrationPhase == 1 || vibrationPhase == 3) ? Config::MOTOR_OFF_LEVEL : Config::MOTOR_ON_LEVEL);
  vibrationDeadline = now + durations[vibrationPhase - 1];
}

void startEmergency(uint32_t now) {
  emergencyActive = true;
  smsQueued = false;
  smsInFlight = false;
  smsFinished = false;
  callPending = false;
  callRequested = false;
  callObservedBusy = false;
  callWasActive = false;
  hangupRequestedByUser = false;
  callAttempts = 0;
  retryCallAt = now;
  screenAwakeUntil = 0;
  currentPage = DisplayPage::Sim;
  display.setPage(currentPage);
  emergencyText = "SOS STARTED";
  display.setAwake(true);
  display.setEmergencyState(true, emergencyText);
  startVibration(now);
}

void finishEmergency(const char *result) {
  emergencyActive = false;
  smsInFlight = false;
  callPending = false;
  callRequested = false;
  emergencyText = result;
  display.setEmergencyState(false, emergencyText);
  screenAwakeUntil = millis() + Config::SCREEN_AWAKE_MS;
}

void updateEmergency(uint32_t now) {
  if (!emergencyActive) return;
  const GsmStatus &status = gsm.status();

  if (smsInFlight && !status.smsBusy) {
    smsInFlight = false;
    smsFinished = true;
    callPending = true;  // Place the call even if SMS submission failed.
    emergencyText = status.smsAccepted ? "SMS ACCEPTED" : "SMS FAILED; CALLING";
    display.setEmergencyState(true, emergencyText);
  }

  if (!smsQueued && status.registered && !status.smsBusy) {
    char message[120];
    const GpsStatus &position = gps.status();
    if (position.gpsFixValid) {
      char latitude[20], longitude[20];
      dtostrf(position.latitude, 0, 6, latitude);
      dtostrf(position.longitude, 0, 6, longitude);
      snprintf(message, sizeof(message), "SOS! I need help. Location: https://maps.google.com/?q=%s,%s",
               latitude, longitude);
    } else {
      snprintf(message, sizeof(message), "SOS! I need help. GPS location is not available yet.");
    }
    if (gsm.requestSms(Config::GUARDIAN_1, message)) {
      smsQueued = true;
      smsInFlight = true;
      emergencyText = "SENDING SOS SMS";
      display.setEmergencyState(true, emergencyText);
    }
  }

  if (!smsQueued) {
    emergencyText = status.registered ? "PREPARING ALERT" : "WAITING FOR NETWORK";
    display.setEmergencyState(true, emergencyText);
    return;
  }
  if (!smsFinished || !callPending || static_cast<int32_t>(now - retryCallAt) < 0) return;

  if (status.registered && !status.callBusy) {
    if (callAttempts >= 2) {
      finishEmergency("CALL DID NOT CONNECT");
      return;
    }
    if (gsm.requestCall(Config::GUARDIAN_1)) {
      ++callAttempts;
      callRequestAt = now;
      callRequested = true;
      callPending = false;
      callObservedBusy = false;
      callWasActive = false;
      emergencyText = callAttempts == 1 ? "CALLING GUARDIAN" : "RETRYING CALL";
      display.setEmergencyState(true, emergencyText);
    }
    return;
  }
  if (!status.registered) {
    emergencyText = "WAITING FOR NETWORK";
    display.setEmergencyState(true, emergencyText);
  }
}

void updateCallState(uint32_t now) {
  if (!emergencyActive || !callRequested) return;
  const GsmStatus &status = gsm.status();
  if (status.callBusy) callObservedBusy = true;
  if (status.callActive) {
    callWasActive = true;
    emergencyText = "CALL CONNECTED";
    display.setEmergencyState(true, emergencyText);
  }

  if (hangupRequestedByUser && !status.callBusy && !status.callActive) {
    finishEmergency("CALL ENDED");
    return;
  }
  if (callObservedBusy && !status.callBusy && !status.callActive) {
    callRequested = false;
    if (callWasActive) {
      finishEmergency("CALL ENDED");
    } else {
      callPending = true;
      retryCallAt = now + Config::CALL_RETRY_COOLDOWN_MS;
      emergencyText = "CALL FAILED; RETRYING";
      display.setEmergencyState(true, emergencyText);
    }
    return;
  }
  if (!callObservedBusy && uint32_t(now - callRequestAt) >= Config::CALL_TIMEOUT_MS) {
    callRequested = false;
    if (callAttempts < 2) {
      callPending = true;
      retryCallAt = now + Config::CALL_RETRY_COOLDOWN_MS;
      emergencyText = "CALL TIMEOUT; RETRY";
      display.setEmergencyState(true, emergencyText);
    } else {
      finishEmergency("CALL FAILED");
    }
  }
}

void updateButtons(uint32_t now) {
  bool wakePressed = false;
  updateButton(wakeButton, Config::WAKE_BUTTON, now, wakePressed);
  if (wakePressed && !emergencyActive) {
    showNextPage(now);
  }

  bool sosPressed = false;
  const bool sosHeld = updateButton(sosButton, Config::SOS_BUTTON, now, sosPressed);
  (void)sosPressed;
  const uint32_t requiredHold = emergencyActive ? Config::END_CALL_HOLD_MS : Config::SOS_HOLD_MS;
  if (sosHeld && !sosButton.longActionSent &&
      uint32_t(now - sosButton.pressedAt) >= requiredHold) {
    sosButton.longActionSent = true;
    if (!emergencyActive) {
      startEmergency(now);
    } else if (gsm.status().callBusy || gsm.status().callActive) {
      gsm.requestHangup();
      hangupRequestedByUser = true;
      emergencyText = "ENDING CALL";
      display.setEmergencyState(true, emergencyText);
    }
  }

  if (!emergencyActive && screenAwakeUntil &&
      static_cast<int32_t>(now - screenAwakeUntil) >= 0) {
    screenAwakeUntil = 0;
    display.setAwake(false);
  }
  updateVibration(now);
}

void sleepUntilButton() {
  if (emergencyActive || screenAwakeUntil) return;
  if (static_cast<int32_t>(millis() - startupRunUntil) < 0) return;
  if (digitalRead(Config::WAKE_BUTTON) == LOW || digitalRead(Config::SOS_BUTTON) == LOW) return;

  const uint64_t wakeMask = (1ULL << Config::WAKE_BUTTON) | (1ULL << Config::SOS_BUTTON);
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  if (esp_sleep_enable_ext1_wakeup_io(wakeMask, ESP_EXT1_WAKEUP_ANY_LOW) != ESP_OK) return;
  esp_light_sleep_start();
  const uint64_t wakeStatus = esp_sleep_get_ext1_wakeup_status();
  if (wakeStatus & (1ULL << Config::WAKE_BUTTON)) {
    currentPage = DisplayPage::Clock;
    display.setPage(currentPage);
    display.setAwake(true);
    screenAwakeUntil = millis() + Config::SCREEN_AWAKE_MS;
    // The pin is already low because this press woke light sleep. Mark it as
    // held so debounce does not count the same physical press a second time.
    wakeButton.rawHigh = wakeButton.stableHigh = (digitalRead(Config::WAKE_BUTTON) == HIGH);
    wakeButton.rawChangedAt = millis();
  }
  if (wakeStatus & (1ULL << Config::SOS_BUTTON)) {
    sosButton.rawHigh = true;
    sosButton.stableHigh = true;
    sosButton.rawChangedAt = millis();
  }
}
}

void setup() {
  pinMode(Config::WAKE_BUTTON, INPUT_PULLUP);
  pinMode(Config::SOS_BUTTON, INPUT_PULLUP);
  pinMode(Config::MOTOR_CONTROL, OUTPUT);
  digitalWrite(Config::MOTOR_CONTROL, Config::MOTOR_OFF_LEVEL);

  const bool wakeHigh = digitalRead(Config::WAKE_BUTTON) == HIGH;
  wakeButton.rawHigh = wakeButton.stableHigh = wakeHigh;
  const bool sosHigh = digitalRead(Config::SOS_BUTTON) == HIGH;
  sosButton.rawHigh = sosButton.stableHigh = sosHigh;

  gps.begin();
  gsm.begin(millis());
  display.begin(millis());
  display.setAwake(false);
  startupRunUntil = millis() + 20000U;
}

void loop() {
  const uint32_t now = millis();
  updateButtons(now);
  gps.update();
  gsm.update(now);
  updateEmergency(now);
  updateCallState(now);
  display.update(now, gps.stateText(now), gsm.status(),
                 gsm.modemText(), gsm.simText(), gsm.networkText(),
                 gsm.callText(), gps.status());
  gps.update();
  gsm.update(millis());
  sleepUntilButton();
}



