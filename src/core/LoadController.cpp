/*
  LoadController.cpp - ver LoadController.h

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#include "LoadController.h"
#include "Logger.h"

extern "C" {
#include <driver/dac.h>
}

LoadController::LoadController()
    : pid_(&pidInput_, &pidOutput_, &pidSetpoint_, 0.05f, 0.06f, 0.03f, PID::DIRECT) {}

void LoadController::begin(const LoadControllerPins &pins) {
  pins_ = pins;

  for (uint8_t i = 0; i < 4; i++) {
    pinMode(pins_.relayPins[i], OUTPUT);
    digitalWrite(pins_.relayPins[i], LOW);
  }

  const LoadControlConfig &load = ConfigStore::get().load;

  ledcAttachPin(pins_.pwmPin, pins_.ledcChannel);
  ledcSetup(pins_.ledcChannel, (double)load.pwmFrequencyHz / 10.0, 10);
  ledcWrite(pins_.ledcChannel, 0);
  dac_output_enable((dac_channel_t)pins_.dacChannel);

  pid_.SetSampleTime(1000);
  load.dimmerLowCost ? pid_.SetOutputLimits(209, load.maxPwmLowCost) : pid_.SetOutputLimits(0, 1023);
  pid_.SetMode(load.manualMode ? PID::MANUAL : PID::AUTOMATIC);
  pid_.SetControllerDirection(ConfigStore::get().surplus.changeGridSign ? PID::DIRECT : PID::REVERSE);
  pid_.SetTunings(load.pid.kp, load.pid.ki, load.pid.kd, PID::P_ON_M);
  pidSetpoint_ = load.potTarget;

  workingMode_ = !load.pwmEnabled ? LoadWorkingMode::Off : (load.manualMode ? LoadWorkingMode::Manual : LoadWorkingMode::Auto);
  running_ = true;
}

void LoadController::onNewReading(const PowerReading &reading, bool sourceConnected) {
  reading_ = reading;
  lastReadingMs_ = millis();
  sourceConnected_ = sourceConnected;
}

void LoadController::checkDataHealth() {
  const AppConfig &cfg = ConfigStore::get();
  bool offGrid = cfg.load.offGrid.enabled;

  if (reading_.gridWatts != lastVariationReading_.gridWatts || offGrid) {
    lastVariationReading_ = reading_;
    lastVariationMs_ = millis();
    variationTimeout_ = false;
  }

  if (!variationTimeout_ && (millis() - lastVariationMs_) > cfg.surplus.maxErrorTimeMs) {
    Logger::info("PWM: a desligar - dados de consumo da rede sem variacao ha demasiado tempo\n");
    reading_ = PowerReading{};
    variationTimeout_ = true;
  }

  if (!connectionTimeout_ && (millis() - lastReadingMs_) > cfg.surplus.maxErrorTimeMs) {
    Logger::info("PWM: a desligar - falha na ligacao com a fonte de dados\n");
    reading_ = PowerReading{};
    connectionTimeout_ = true;
  }

  if ((connectionTimeout_ || variationTimeout_ || !running_) && pid_.GetMode() == PID::AUTOMATIC) {
    shutdown(true, "PWM: desligado por motivos de seguranca\n");
  }
}

void LoadController::applyManualRamp() {
  const LoadControlConfig &load = ConfigStore::get().load;
  bool changeSign = ConfigStore::get().surplus.changeGridSign;

  if (!((load.manualMode || manualAutoOverride_) && load.pwmEnabled && running_)) return;

  uint16_t tariffOffset = (uint16_t)(load.maxWattsTariff * 0.10f);
  uint16_t maxTargetPwm = targetPwmForPercent(load.manualControlPercent);

  bool belowTariff = changeSign ? (reading_.gridWatts < (float)(load.maxWattsTariff - tariffOffset))
                                 : (reading_.gridWatts > -(float)(load.maxWattsTariff - tariffOffset));

  if (belowTariff && targetPwm_ < maxTargetPwm) {
    targetPwm_ = (targetPwm_ < maxTargetPwm - 8) ? (uint16_t)(targetPwm_ + 8) : maxTargetPwm;
    if (load.dimmerLowCost && targetPwm_ < 210) targetPwm_ = 210;
  }

  bool aboveTariff = changeSign ? (reading_.gridWatts > (float)load.maxWattsTariff)
                                 : (reading_.gridWatts < -(float)load.maxWattsTariff);

  if (aboveTariff || targetPwm_ > maxTargetPwm) {
    targetPwm_ = (targetPwm_ >= 8) ? (uint16_t)(targetPwm_ - 8) : 0;
    if (load.dimmerLowCost && targetPwm_ < 210) targetPwm_ = 0;
  }

  if (pwmValue_ != targetPwm_) { writePwm(targetPwm_); }
}

void LoadController::applyAutoPidGate() {
  const LoadControlConfig &load = ConfigStore::get().load;
  bool changeSign = ConfigStore::get().surplus.changeGridSign;

  if (!(load.pwmEnabled && !load.manualMode && !manualAutoOverride_ && !variationTimeout_ && !connectionTimeout_ && running_)) return;

  bool shouldEnable;
  if (load.offGrid.enabled) {
    shouldEnable = load.offGrid.useVoltage
                       ? (pid_.GetMode() == PID::MANUAL && reading_.batteryWatts > load.potTarget && reading_.gridVoltage >= load.offGrid.batteryVoltage)
                       : (pid_.GetMode() == PID::MANUAL && reading_.batteryWatts > load.potTarget && reading_.batterySoc >= load.offGrid.socThreshold);
  } else {
    shouldEnable = pid_.GetMode() == PID::MANUAL &&
                   (changeSign ? reading_.gridWatts < load.potTarget : reading_.gridWatts > load.potTarget) &&
                   reading_.batteryWatts >= load.offGrid.battWattsThreshold;
  }

  if (shouldEnable) {
    pid_.SetMode(PID::AUTOMATIC);
    pid_.SetControllerDirection(load.offGrid.enabled ? PID::REVERSE : (changeSign ? PID::DIRECT : PID::REVERSE));
    pidSetpoint_ = load.potTarget;
    return;
  }

  bool shouldDisable;
  if (load.offGrid.enabled) {
    shouldDisable = load.offGrid.useVoltage
                         ? (pid_.GetMode() == PID::AUTOMATIC && (reading_.gridVoltage < (load.offGrid.batteryVoltage - load.offGrid.voltageOffset) || (pwmValue_ == 0 && reading_.batteryWatts < load.potTarget)))
                         : (pid_.GetMode() == PID::AUTOMATIC && (reading_.batterySoc < load.offGrid.socThreshold || (pwmValue_ == 0 && reading_.batteryWatts < load.potTarget)));
  } else {
    shouldDisable = pid_.GetMode() == PID::AUTOMATIC &&
                     (reading_.batteryWatts < load.offGrid.battWattsThreshold ||
                      (pwmValue_ == 0 && (changeSign ? reading_.gridWatts > load.potTarget : reading_.gridWatts < load.potTarget)));
  }

  if (shouldDisable) shutdown(false);
}

void LoadController::applyRelayStaging() {
  const LoadControlConfig &load = ConfigStore::get().load;
  bool changeSign = ConfigStore::get().surplus.changeGridSign;
  bool autoMode = !load.manualMode;

  // Arranque escalonado (só em modo automático), prioridade da saída 1 para a 4
  if (autoMode && load.pwmEnabled) {
    for (uint8_t i = 0; i < 4 && canTurnOnRelay_; i++) {
      const RelayOutputConfig &out = load.outputs[i];
      if (digitalRead(pins_.relayPins[i])) continue;

      bool percentMode = out.minPercent != 999;
      bool trigger = percentMode
                          ? (pwmPercent_ >= out.minPercent)
                          : (pwmPercent_ >= load.autoControlPercent &&
                             (changeSign ? reading_.gridWatts < out.onWatts : reading_.gridWatts > out.onWatts));

      if (trigger) {
        digitalWrite(pins_.relayPins[i], HIGH);
        relayState_[i].energized = true;
        relayState_[i].autoActivated = true;
        canTurnOnRelay_ = false;
        relayOnLockUntil_ = millis() + RELAY_STAGGER_MS;
        Logger::info("Saida %d: ligada automaticamente\n", i + 1);
      }
    }
  }

  // Paragem escalonada (em qualquer modo), prioridade da saída 4 para a 1
  for (int8_t i = 3; i >= 0 && canTurnOffRelay_; i--) {
    const RelayOutputConfig &out = load.outputs[i];
    if (relayManual_[i]) continue;
    if (!digitalRead(pins_.relayPins[i])) continue;

    bool percentMode = out.minPercent != 999;
    bool trigger;
    if (percentMode) {
      uint16_t offThreshold = out.minPercent >= 10 ? out.minPercent - 10 : 0;
      trigger = pwmPercent_ <= offThreshold;
    } else {
      trigger = changeSign ? (reading_.gridWatts > out.offWatts) : (reading_.gridWatts < out.offWatts);
    }

    if (trigger) {
      digitalWrite(pins_.relayPins[i], LOW);
      relayState_[i].energized = false;
      relayState_[i].autoActivated = false;
      canTurnOffRelay_ = false;
      relayOffLockUntil_ = millis() + RELAY_STAGGER_MS;
      Logger::info("Saida %d: desligada automaticamente\n", i + 1);
    }
  }
}

void LoadController::applyManualOverrides(bool forceOff) {
  for (uint8_t i = 0; i < 4; i++) {
    bool pinOn = digitalRead(pins_.relayPins[i]);

    if (relayManual_[i] && relayManualState_[i] && !pinOn) {
      digitalWrite(pins_.relayPins[i], HIGH);
      relayState_[i].energized = true;
      Logger::info("Saida %d: forcada manualmente ON\n", i + 1);
    }

    // Só desliga automaticamente uma saída que não esteja ativa de forma
    // manual nem tenha sido ativada pelo escalonamento automático — evita
    // que uma reconfiguração noutra saída desligue uma carga em curso.
    if ((!relayManual_[i] || !relayManualState_[i]) && pinOn && !relayState_[i].autoActivated) {
      digitalWrite(pins_.relayPins[i], LOW);
      relayState_[i].energized = false;
      Logger::info("Saida %d: forcada OFF\n", i + 1);
    }

    if (forceOff) {
      digitalWrite(pins_.relayPins[i], LOW);
      relayState_[i].energized = false;
      relayState_[i].autoActivated = false;
    }
  }
}

void LoadController::writePwm(uint16_t value) {
  const LoadControlConfig &load = ConfigStore::get().load;
  if (load.dimmerLowCost && value > 1023) value -= 1023;

  pwmValue_ = value;
  ledcWrite(pins_.ledcChannel, value);
  dac_output_enable((dac_channel_t)pins_.dacChannel);
  dac_output_voltage((dac_channel_t)pins_.dacChannel, constrain(value / 4, 0, 255));
  recalcPwmPercent();
}

uint16_t LoadController::targetPwmForPercent(uint16_t percent) const {
  const LoadControlConfig &load = ConfigStore::get().load;
  if (load.dimmerLowCost) {
    uint16_t maxTargetPwm = (uint16_t)((((load.maxPwmLowCost - 210) * percent) / 100) + 210);
    return maxTargetPwm <= 210 ? 0 : maxTargetPwm;
  }
  return (uint16_t)((1023 * percent) / 100);
}

void LoadController::recalcPwmPercent() {
  const LoadControlConfig &load = ConfigStore::get().load;
  if (load.dimmerLowCost) {
    pwmPercent_ = pwmValue_ > 0 ? (uint8_t)round(((float)(pwmValue_ - 210) * 100.0f) / (load.maxPwmLowCost - 210.0f)) : 0;
  } else {
    pwmPercent_ = (uint8_t)round((pwmValue_ * 100.0f) / 1023.0f);
  }
}

void LoadController::update() {
  const LoadControlConfig &load = ConfigStore::get().load;

  pidInput_ = load.offGrid.enabled ? reading_.batteryWatts : reading_.gridWatts;

  checkDataHealth();
  applyManualRamp();
  applyAutoPidGate();

  if (load.pwmEnabled && !variationTimeout_ && !connectionTimeout_ && running_ && pid_.GetMode() == PID::AUTOMATIC && pid_.Compute()) {
    uint16_t newValue = (uint16_t)pidOutput_;
    if (load.dimmerLowCost && newValue <= 210) newValue = 0;
    targetPwm_ = newValue;
    writePwm(newValue);
  }

  applyRelayStaging();

  if (load.potManPwmActive) {
    if (reading_.solarWatts < load.potManPwmWatts && !manualAutoOverride_) {
      manualAutoOverride_ = true;
    }
    if (reading_.solarWatts > load.potManPwmWatts && manualAutoOverride_) {
      manualAutoOverride_ = false;
    }
  }

  if (!canTurnOnRelay_ && millis() >= relayOnLockUntil_) canTurnOnRelay_ = true;
  if (!canTurnOffRelay_ && millis() >= relayOffLockUntil_) canTurnOffRelay_ = true;
}

void LoadController::shutdown(bool forceRelaysOff, const char *reason) {
  if (reason && reason[0]) Logger::info("%s", reason);

  pid_.SetMode(PID::MANUAL);
  pidOutput_ = 0;
  pidSetpoint_ = 0;
  targetPwm_ = 0;
  writePwm(0);

  applyManualOverrides(forceRelaysOff);
}

void LoadController::setWorkingMode(LoadWorkingMode mode) {
  LoadControlConfig &load = ConfigStore::get().load;
  workingMode_ = mode;

  switch (mode) {
    case LoadWorkingMode::Auto:
      load.pwmEnabled = true;
      load.manualMode = false;
      running_ = true;
      pid_.SetMode(PID::AUTOMATIC);
      pid_.SetCurrentOutput(pwmValue_);
      pidSetpoint_ = load.potTarget;
      break;
    case LoadWorkingMode::Manual:
      load.pwmEnabled = true;
      load.manualMode = true;
      running_ = true;
      pid_.SetMode(PID::MANUAL);
      pidOutput_ = 0;
      pidSetpoint_ = 0;
      break;
    case LoadWorkingMode::Off:
      load.pwmEnabled = false;
      load.manualMode = false;
      shutdown(false, "PWM: modo OFF\n");
      break;
  }

  ConfigStore::save();
}

void LoadController::toggleWorkingMode() {
  switch (workingMode_) {
    case LoadWorkingMode::Auto: setWorkingMode(LoadWorkingMode::Manual); break;
    case LoadWorkingMode::Manual: setWorkingMode(LoadWorkingMode::Off); break;
    case LoadWorkingMode::Off: setWorkingMode(LoadWorkingMode::Auto); break;
  }
}

void LoadController::setRelayManual(uint8_t index, bool forcedOn) {
  if (index >= 4) return;
  relayManual_[index] = true;
  relayManualState_[index] = forcedOn;
  applyManualOverrides(false);
}

void LoadController::setRelayAuto(uint8_t index) {
  if (index >= 4) return;
  relayManual_[index] = false;
  applyManualOverrides(false);
}
