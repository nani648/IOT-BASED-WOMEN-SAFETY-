#include "gsm.h"

#include <HardwareSerial.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "config.h"

void GsmModem::begin(uint32_t now) {
  // ESP32-S3 UART2 for modem. Native USB Serial remains debug; UART1 is GPS.
  static HardwareSerial gsmSerial(2);
  serial_ = &gsmSerial;
  serial_->begin(Config::GSM_BAUD, SERIAL_8N1, Config::GSM_RX, Config::GSM_TX);
  nextAttempt_ = now;
}

void GsmModem::sendCommand(const char *command, uint32_t now) {
  if (!serial_) return;
  while (serial_->available()) serial_->read();  // discard stale response before transaction
  serial_->print(command);
  serial_->print("\r\n");
  snprintf(status_.lastCommand, sizeof(status_.lastCommand), "%s", command);
  deadline_ = now + Config::GSM_RESPONSE_TIMEOUT_MS;
  if (Serial && manualTransaction_) Serial.printf("[TX] %s\n", command);
}

void GsmModem::readModemLines(uint32_t now) {
  if (!serial_) return;
  while (serial_->available()) {
    const char c = static_cast<char>(serial_->read());
    if (step_ == GsmStep::WaitSmsPrompt && c == '>') {
      serial_->print(smsBody_);
      serial_->write(static_cast<uint8_t>(26));
      snprintf(status_.lastCommand, sizeof(status_.lastCommand), "SMS TEXT + Ctrl-Z");
      step_ = GsmStep::WaitSmsResult;
      deadline_ = now + Config::SMS_TIMEOUT_MS;
      if (Serial && manualTransaction_) Serial.println("[MODEM] SMS prompt received; text submitted");
      continue;
    }
    if (c != '\n' && c != '\r') {
      if (lineLength_ + 1 < sizeof(line_)) line_[lineLength_++] = c;
      else lineLength_ = 0;  // overlong line: discard it safely
      continue;
    }
    if (!lineLength_) continue;
    line_[lineLength_] = '\0';
    if (Serial && manualTransaction_) Serial.printf("[RX] %s\n", line_);
    snprintf(status_.lastResponse, sizeof(status_.lastResponse), "%s", line_);
    if (strcmp(line_, "NO CARRIER") == 0 || strcmp(line_, "BUSY") == 0 ||
        strcmp(line_, "NO ANSWER") == 0) {
      status_.callActive = false;
      status_.callBusy = false;
      callInProgress_ = false;
      if (Serial && manualTransaction_) Serial.printf("[RESULT] Call ended: %s\n", line_);
      step_ = GsmStep::StartAt;
      nextAttempt_ = now + Config::GSM_REGISTER_POLL_MS;
      manualTransaction_ = false;
    } else if (step_ == GsmStep::WaitCallList && strncmp(line_, "+CLCC:", 6) == 0) {
      int index = 0, direction = 0, state = -1;
      if (sscanf(line_ + 6, " %d,%d,%d", &index, &direction, &state) == 3) {
        gotCallEntry_ = true;
        status_.callActive = state == 0;
      }
    } else if (step_ == GsmStep::WaitSmsResult && strncmp(line_, "+CMGS:", 6) == 0) {
      gotSmsReference_ = true;
    } else if (strcmp(line_, "OK") == 0 && step_ == GsmStep::WaitSmsMode) {
      step_ = GsmStep::StartSmsNumber; nextAttempt_ = now + 200;
    } else if (strcmp(line_, "OK") == 0 && step_ == GsmStep::WaitSmsResult) {
      status_.smsBusy = false;
      status_.smsAccepted = gotSmsReference_;
      if (Serial && manualTransaction_) Serial.println(gotSmsReference_ ? "[RESULT] SMS accepted by modem (+CMGS)" : "[RESULT] SMS failed: no +CMGS reference");
      manualTransaction_ = false;
      step_ = GsmStep::StartAt;
      nextAttempt_ = now + Config::GSM_REGISTER_POLL_MS;
    } else if (strcmp(line_, "OK") == 0 && step_ == GsmStep::WaitDial) {
      callInProgress_ = true;
      status_.callBusy = true;
      nextCallPoll_ = now + Config::CALL_POLL_MS;
      step_ = GsmStep::StartAt;
      if (Serial && manualTransaction_) Serial.println("[RESULT] Dial accepted; waiting for call state");
    } else if (strcmp(line_, "OK") == 0 && step_ == GsmStep::WaitHangup) {
      callInProgress_ = false;
      status_.callActive = false;
      status_.callBusy = false;
      step_ = GsmStep::StartAt;
      nextAttempt_ = now + Config::GSM_REGISTER_POLL_MS;
    } else if (strcmp(line_, "OK") == 0 && step_ == GsmStep::WaitCallList) {
      if (!gotCallEntry_) {
        callInProgress_ = false;
        status_.callActive = false;
        status_.callBusy = false;
        if (Serial && manualTransaction_) Serial.println("[RESULT] No active call reported");
      }
      step_ = GsmStep::StartAt;
      nextCallPoll_ = now + Config::CALL_POLL_MS;
    } else if (step_ == GsmStep::WaitAt && strcmp(line_, "OK") == 0) {
      status_.modemResponding = true;
      ++status_.successfulAtTests;
      step_ = GsmStep::StartSim;
      nextAttempt_ = now + 300;
    } else if (step_ == GsmStep::WaitSim && strncmp(line_, "+CPIN:", 6) == 0) {
      const char *state = line_ + 6;
      while (*state == ' ') ++state;
      status_.simStatusKnown = true;
      status_.simReady = strcmp(state, "READY") == 0;
      status_.simPinRequired = strcmp(state, "SIM PIN") == 0 || strcmp(state, "SIM PUK") == 0;
      status_.simQueryFailed = false;
      step_ = GsmStep::StartRegistration;
      nextAttempt_ = now + 300;
    } else if (step_ == GsmStep::WaitRegistration &&
               strncmp(line_, "+CREG:", 6) == 0) {
      const char *comma = strrchr(line_, ',');
      const int code = comma ? atoi(comma + 1) : -1;
      status_.registrationStatus = code;
      status_.registrationKnown = code >= 0;
      status_.registered = code == 1 || code == 5;  // home or roaming
      step_ = GsmStep::StartAt;
      nextAttempt_ = now + Config::GSM_REGISTER_POLL_MS;
    } else if (strcmp(line_, "ERROR") == 0) {
      if (step_ == GsmStep::WaitSmsMode || step_ == GsmStep::WaitSmsPrompt ||
          step_ == GsmStep::WaitSmsResult) {
        status_.smsBusy = false; status_.smsAccepted = false;
        step_ = GsmStep::StartAt; nextAttempt_ = now + Config::GSM_RETRY_MS;
        if (Serial && manualTransaction_) Serial.println("[RESULT] Modem rejected SMS transaction");
        manualTransaction_ = false;
      } else if (step_ == GsmStep::WaitAt) {
        status_.modemResponding = false;
        step_ = GsmStep::StartAt;
        nextAttempt_ = now + Config::GSM_RETRY_MS;
      } else if (step_ == GsmStep::WaitSim) {
        // Generic ERROR does not say whether the SIM is absent or the query failed.
        // Report that honestly, then still ask the modem for network status.
        status_.simStatusKnown = true;
        status_.simReady = false;
        status_.simPinRequired = false;
        status_.simQueryFailed = true;
        step_ = GsmStep::StartRegistration;
        nextAttempt_ = now + 300;
      } else {
        status_.registrationKnown = false;
        step_ = GsmStep::StartAt;
        nextAttempt_ = now + Config::GSM_RETRY_MS;
      }
    }
    lineLength_ = 0;
  }
}

void GsmModem::update(uint32_t now) {
  readModemLines(now);
  if (static_cast<int32_t>(now - deadline_) < 0 &&
      (step_ == GsmStep::WaitAt || step_ == GsmStep::WaitSim ||
       step_ == GsmStep::WaitRegistration || step_ == GsmStep::WaitSmsMode ||
       step_ == GsmStep::WaitSmsPrompt ||
       step_ == GsmStep::WaitSmsResult || step_ == GsmStep::WaitDial ||
       step_ == GsmStep::WaitCallList || step_ == GsmStep::WaitHangup)) return;
  if (step_ == GsmStep::WaitAt || step_ == GsmStep::WaitSim ||
      step_ == GsmStep::WaitRegistration) {
    if (step_ == GsmStep::WaitAt) {
      status_.modemResponding = false;
      status_.registrationKnown = false;
      if (Serial && manualTransaction_) Serial.println("[RESULT] AT timeout");
    } else if (step_ == GsmStep::WaitSim) {
      status_.simStatusKnown = false;
      if (Serial && manualTransaction_) Serial.println("[RESULT] SIM query timeout");
    } else {
      status_.registrationKnown = false;
      if (Serial && manualTransaction_) Serial.println("[RESULT] Registration query timeout");
    }
    step_ = GsmStep::StartAt;
    nextAttempt_ = now + Config::GSM_RETRY_MS;
  } else if (step_ == GsmStep::WaitSmsMode || step_ == GsmStep::WaitSmsPrompt ||
             step_ == GsmStep::WaitSmsResult || step_ == GsmStep::WaitDial ||
             step_ == GsmStep::WaitHangup || step_ == GsmStep::WaitCallList) {
      if (Serial && manualTransaction_) Serial.println("[RESULT] Modem operation timed out");
    status_.smsBusy = false; status_.smsAccepted = false;
    if (step_ == GsmStep::WaitDial || step_ == GsmStep::WaitHangup) {
      callInProgress_ = false; status_.callBusy = false; status_.callActive = false;
    }
    step_ = GsmStep::StartAt;
    nextAttempt_ = now + Config::GSM_RETRY_MS;
    manualTransaction_ = false;
  }
  if (static_cast<int32_t>(now - nextAttempt_) < 0) return;
  if (callInProgress_ && static_cast<int32_t>(now - nextCallPoll_) >= 0 && step_ == GsmStep::StartAt) {
    gotCallEntry_ = false; sendCommand("AT+CLCC", now); step_ = GsmStep::WaitCallList; return;
  }
  if (step_ == GsmStep::StartAt) {
    sendCommand("AT", now);
    step_ = GsmStep::WaitAt;
  } else if (step_ == GsmStep::StartSim) {
    sendCommand("AT+CPIN?", now);
    step_ = GsmStep::WaitSim;
  } else if (step_ == GsmStep::StartRegistration) {
    sendCommand("AT+CREG?", now);
    step_ = GsmStep::WaitRegistration;
  } else if (step_ == GsmStep::StartSmsMode) {
    sendCommand("AT+CMGF=1", now); step_ = GsmStep::WaitSmsMode;
  } else if (step_ == GsmStep::StartSmsNumber) {
    char command[48]; snprintf(command, sizeof(command), "AT+CMGS=\"%s\"", phone_);
    sendCommand(command, now); step_ = GsmStep::WaitSmsPrompt;
  }
}

bool GsmModem::requestSms(const char *number, const char *message) {
  if (!serial_ || !status_.registered || status_.smsBusy || callInProgress_ ||
      step_ != GsmStep::StartAt || strlen(number) >= sizeof(phone_) || strlen(message) >= sizeof(smsBody_)) return false;
  strcpy(phone_, number); strcpy(smsBody_, message);
  manualTransaction_ = true;
  snprintf(status_.lastAction, sizeof(status_.lastAction), "SMS1 QUEUED");
  status_.smsBusy = true; status_.smsAccepted = false;
  step_ = GsmStep::StartSmsMode; nextAttempt_ = millis();
  return true;
}

bool GsmModem::requestCall(const char *number) {
  if (!serial_ || !status_.registered || callInProgress_ || step_ != GsmStep::StartAt ||
      strlen(number) >= sizeof(phone_)) return false;
  strcpy(phone_, number);
  manualTransaction_ = true;
  snprintf(status_.lastAction, sizeof(status_.lastAction), "CALL1 REQUESTED");
  char command[48]; snprintf(command, sizeof(command), "ATD%s;", phone_);
  sendCommand(command, millis()); step_ = GsmStep::WaitDial;
  return true;
}

void GsmModem::requestHangup() {
  if (!callInProgress_ || !serial_) return;
  manualTransaction_ = true;
  snprintf(status_.lastAction, sizeof(status_.lastAction), "HANGUP REQUESTED");
  sendCommand("ATH", millis()); step_ = GsmStep::WaitHangup;
}

void GsmModem::recordAction(const char *action) {
  snprintf(status_.lastAction, sizeof(status_.lastAction), "%s", action);
  if (strcmp(action, "HELP RECEIVED") == 0 || strcmp(action, "UNKNOWN COMMAND") == 0)
    manualTransaction_ = false;
}

const char *GsmModem::callText() const {
  if (status_.callActive) return "CALL ACTIVE";
  if (callInProgress_ || status_.callBusy) return "DIALING / RINGING";
  return "IDLE";
}

const char *GsmModem::simText() const {
  if (!status_.modemResponding) return "MODEM NOT READY";
  if (!status_.simStatusKnown) return "CHECKING SIM";
  if (status_.simReady) return "SIM READY";
  if (status_.simPinRequired) return "SIM PIN REQUIRED";
  if (status_.simQueryFailed) return "SIM QUERY ERROR";
  return "SIM NOT READY/ABSENT";
}

const char *GsmModem::modemText() const {
  return status_.modemResponding ? "AT OK" : "NO AT RESPONSE";
}

const char *GsmModem::networkText() const {
  if (!status_.modemResponding) return "MODEM NOT READY";
  if (!status_.registrationKnown) return "CHECKING NETWORK";
  if (status_.registered) return status_.registrationStatus == 5 ? "ROAMING" : "REGISTERED";
  switch (status_.registrationStatus) {
    case 0: return "NOT REGISTERED";
    case 2: return "SEARCHING";
    case 3: return "REGISTRATION DENIED";
    default: return "NO NETWORK STATUS";
  }
}

