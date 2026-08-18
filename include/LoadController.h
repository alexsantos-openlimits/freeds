/*
  LoadController.h - Controlo de excedente (PWM + relés escalonados)

  Substitui pwm.ino: a mesma lógica de controlo (PID em modo automático,
  rampa por histerese em modo manual, arranque/paragem escalonado dos 4
  relés por percentagem ou por potência) é agora uma classe única, com um
  ciclo por saída em vez de quatro blocos de código quase idênticos.

  O hardware controlado mantém-se inalterado: 1 saída PWM/DAC para o
  triac/dimmer e 4 saídas de relé, nos mesmos pinos da placa FreeDS
  original (ver LoadControllerPins em FreeDS.ino).

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include <Arduino.h>
#include <PID_v1.h>
#include "AppConfig.h"
#include "Types.h"

struct LoadControllerPins {
  uint8_t pwmPin;
  uint8_t dacChannel;   // DAC_CHANNEL_1 ou DAC_CHANNEL_2
  uint8_t ledcChannel;
  uint8_t relayPins[4];
};

class LoadController {
public:
  LoadController();

  void begin(const LoadControllerPins &pins);

  // A chamar sempre que uma nova leitura de potência estiver disponível.
  void onNewReading(const PowerReading &reading, bool sourceConnected);

  // A chamar periodicamente (recomendado: a cada 500 ms) no loop principal.
  void update();

  // Desliga o PWM imediatamente por segurança (perda de dados, arranque, ...).
  void shutdown(bool forceRelaysOff, const char *reason = "");

  void setRelayManual(uint8_t index, bool forcedOn);
  void setRelayAuto(uint8_t index);
  bool relayIsManual(uint8_t index) const { return relayManual_[index]; }
  void resyncManualRelays() { applyManualOverrides(false); }

  // Alterna Auto -> Manual -> Off -> Auto (botão físico / API)
  void toggleWorkingMode();
  void setWorkingMode(LoadWorkingMode mode);
  LoadWorkingMode workingMode() const { return workingMode_; }

  uint8_t pwmPercent() const { return pwmPercent_; }
  uint16_t pwmRaw() const { return pwmValue_; }
  bool hasDataFault() const { return variationTimeout_ || connectionTimeout_; }
  bool isRunning() const { return running_; }
  void setRunning(bool running) { running_ = running; }
  bool isAutomaticMode() const { return pid_.GetMode() == PID::AUTOMATIC; }
  bool isManualAutoOverride() const { return manualAutoOverride_; }
  const RelayOutputState &relayState(uint8_t i) const { return relayState_[i]; }

private:
  void checkDataHealth();
  void applyManualRamp();
  void applyAutoPidGate();
  void applyRelayStaging();
  void applyManualOverrides(bool forceOff);
  void writePwm(uint16_t value);
  uint16_t targetPwmForPercent(uint16_t percent) const;
  void recalcPwmPercent();

  LoadControllerPins pins_{};

  float pidInput_ = 0, pidOutput_ = 0, pidSetpoint_ = 0;
  PID pid_;

  PowerReading reading_{};
  PowerReading lastVariationReading_{};
  unsigned long lastVariationMs_ = 0;
  unsigned long lastReadingMs_ = 0;
  bool sourceConnected_ = false;
  bool variationTimeout_ = true;
  bool connectionTimeout_ = true;
  bool running_ = false; // equivalente a Flags.pwmIsWorking

  LoadWorkingMode workingMode_ = LoadWorkingMode::Auto;
  bool manualAutoOverride_ = false; // baixa produção solar força modo manual temporariamente

  uint16_t targetPwm_ = 0;
  uint16_t pwmValue_ = 0;
  uint8_t pwmPercent_ = 0;

  RelayOutputState relayState_[4];
  bool relayManual_[4] = {false, false, false, false};
  bool relayManualState_[4] = {false, false, false, false};
  bool canTurnOnRelay_ = true;
  bool canTurnOffRelay_ = true;
  unsigned long relayOnLockUntil_ = 0;
  unsigned long relayOffLockUntil_ = 0;
  static const unsigned long RELAY_STAGGER_MS = 5000;
};
