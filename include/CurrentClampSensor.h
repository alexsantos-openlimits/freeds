/*
  CurrentClampSensor.h - Estimativa da potência da carga controlada pelo PWM

  Substitui current()/calcIrms()/readClamp() em Support_functions.ino. Usa
  a pinça amperimétrica ligada ao ADC (pino 34, inalterado) quando
  SurplusManagerConfig::useClamp está ativo; caso contrário estima a
  potência a partir da curva de um dimmer de fase (seno ao quadrado) e da
  potência nominal configurada da carga (attachedLoadWatts) - tal como no
  firmware original.
*/
#pragma once

#include <Arduino.h>

class CurrentClampSensor {
public:
  void begin(uint8_t adcPin);

  // Chamar periodicamente (o cálculo de Irms demora ~130ms quando a pinça
  // está ativa, tal como no original, por isso não deve ser chamado a cada
  // volta do loop principal).
  void loop(uint8_t pwmPercent);

  float lastWatts() const { return lastWatts_; }

private:
  double readIrms(unsigned int numberOfSamples);

  uint8_t pin_ = 34;
  double offsetI_ = 2048.0;
  float lastWatts_ = 0;
  unsigned long lastReadMs_ = 0;
};
