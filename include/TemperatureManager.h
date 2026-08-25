/*
  TemperatureManager.h - Leitura de sensores DS18B20 e corte de segurança
  por temperatura (termoacumulador / triac)

  Substitui tempsensor.ino. Mantém o mesmo barramento OneWire e a mesma
  lógica de decisão (modos 0-3), agora encapsulada numa classe e ligada ao
  novo LoadController em vez de manipular variáveis globais diretamente.
*/
#pragma once

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "AppConfig.h"

class LoadController;

class TemperatureManager {
public:
  ~TemperatureManager();

  void begin(uint8_t oneWirePin, LoadController *loadController);
  void loop(); // chamar a cada ~1000ms

  float thermoTemperature() const { return thermo_; }
  float triacTemperature() const { return triac_; }
  float customTemperature() const { return custom_; }
  bool thermoSensorError() const { return thermoError_; }

  // Sensores DS18B20 detetados no barramento, para a interface web escolher
  // qual usar como termo/triac/personalizado.
  void scanSensors();
  uint8_t sensorCount() const { return sensorCount_; }
  const uint8_t *sensorAddress(uint8_t index) const { return sensorAddresses_[index]; }

private:
  void readSensors();
  void applyControl();

  OneWire *oneWire_ = nullptr;
  DallasTemperature *sensors_ = nullptr;
  LoadController *loadController_ = nullptr;

  float thermo_ = -127, triac_ = -127, custom_ = -127;
  bool thermoError_ = false, triacError_ = false, customError_ = false;
  unsigned long lastThermoOkMs_ = 0, lastTriacOkMs_ = 0, lastCustomOkMs_ = 0;

  uint8_t sensorAddresses_[15][8] = {};
  uint8_t sensorCount_ = 0;
};
