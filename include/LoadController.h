/*
  LoadController.h - Controlo de excedente (PWM + relés escalonados)

  Substitui pwm.ino: a mesma lógica de controlo (PID em modo automático,
  rampa por histerese em modo manual, arranque/paragem escalonado dos 4
  relés por percentagem ou por potência) é agora uma classe única, com um
  ciclo por saída em vez de quatro blocos de código quase idênticos.

  O hardware controlado mantém-se inalterado: 1 saída PWM/DAC para o
  triac/dimmer e 4 saídas de relé, nos mesmos pinos da placa Lusol
  original (ver LoadControllerPins em Lusol.ino).
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

  // A chamar depois de gravar a configuração de "Controlo de Carga" (PID,
  // frequência do PWM, dimmer económico) ou "Gestor de Excedentes" (sinal
  // da rede) - sem isto, essas alterações só tinham efeito depois de
  // reiniciar o dispositivo.
  void reloadTunables();

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
  // Não pode ser const: PID::GetMode() (lib/Arduino-PID-Library-master) não
  // é const, e chamá-lo sobre pid_ dentro de um método const do LoadController
  // tornaria pid_ implicitamente const, o que não compila.
  bool isAutomaticMode() { return pid_.GetMode() == PID::AUTOMATIC; }
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
