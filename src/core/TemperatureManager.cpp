#include "TemperatureManager.h"
#include "LoadController.h"
#include "Logger.h"

TemperatureManager::~TemperatureManager() {
  delete sensors_;
  delete oneWire_;
}

void TemperatureManager::begin(uint8_t oneWirePin, LoadController *loadController) {
  loadController_ = loadController;
  oneWire_ = new OneWire(oneWirePin);
  sensors_ = new DallasTemperature(oneWire_);
  sensors_->begin();
  sensors_->setResolution(9);
  scanSensors();

  unsigned long now = millis();
  lastThermoOkMs_ = lastTriacOkMs_ = lastCustomOkMs_ = now;
}

void TemperatureManager::scanSensors() {
  sensorCount_ = 0;
  oneWire_->reset_search();
  while (sensorCount_ < 15 && oneWire_->search(sensorAddresses_[sensorCount_])) {
    Logger::info("DS18B20 ID %i: %02X%02X%02X%02X%02X%02X%02X%02X\n", sensorCount_ + 1,
                 sensorAddresses_[sensorCount_][0], sensorAddresses_[sensorCount_][1],
                 sensorAddresses_[sensorCount_][2], sensorAddresses_[sensorCount_][3],
                 sensorAddresses_[sensorCount_][4], sensorAddresses_[sensorCount_][5],
                 sensorAddresses_[sensorCount_][6], sensorAddresses_[sensorCount_][7]);
    sensorCount_++;
  }
}

void TemperatureManager::readSensors() {
  const TemperatureConfig &cfg = ConfigStore::get().temperature;
  uint32_t maxErrorMs = ConfigStore::get().surplus.maxErrorTimeMs;

  sensors_->setWaitForConversion(true);
  sensors_->requestTemperatures();

  if (cfg.thermoSensorAddr[0] != 0x0) {
    float t = sensors_->getTempC(const_cast<uint8_t *>(cfg.thermoSensorAddr));
    if (t != -127.0f) {
      thermo_ = t;
      lastThermoOkMs_ = millis();
      thermoError_ = false;
    }
    if ((millis() - lastThermoOkMs_) > maxErrorMs) {
      thermoError_ = true;
      thermo_ = -127;
      Logger::info("Falha ao ler a temperatura do termoacumulador (DS18B20)\n");
    }
  } else {
    thermo_ = -127;
    thermoError_ = false;
  }

  if (cfg.triacSensorAddr[0] != 0x0) {
    float t = sensors_->getTempC(const_cast<uint8_t *>(cfg.triacSensorAddr));
    if (t != -127.0f) {
      triac_ = t;
      lastTriacOkMs_ = millis();
      triacError_ = false;
    }
    if ((millis() - lastTriacOkMs_) > maxErrorMs) {
      triacError_ = true;
      triac_ = -127;
      Logger::info("Falha ao ler a temperatura do triac (DS18B20)\n");
    }
  } else {
    triac_ = -127;
    triacError_ = false;
  }

  if (cfg.customSensorAddr[0] != 0x0) {
    float t = sensors_->getTempC(const_cast<uint8_t *>(cfg.customSensorAddr));
    if (t != -127.0f) {
      custom_ = t;
      lastCustomOkMs_ = millis();
      customError_ = false;
    }
    if ((millis() - lastCustomOkMs_) > maxErrorMs) {
      customError_ = true;
      custom_ = -127;
      Logger::info("Falha ao ler o sensor de temperatura personalizado (DS18B20)\n");
    }
  } else {
    custom_ = -127;
    customError_ = false;
  }
}

void TemperatureManager::applyControl() {
  const TemperatureConfig &cfg = ConfigStore::get().temperature;
  uint32_t maxErrorMs = ConfigStore::get().surplus.maxErrorTimeMs;

  if (!thermoError_) {
    bool manualActive = ConfigStore::get().load.manualMode || loadController_->isManualAutoOverride();

    switch (cfg.mode) {
      case 1: // Automático
        if (!ConfigStore::get().load.manualMode && thermo_ < cfg.turnOnC) { loadController_->setRunning(true); }
        if (!ConfigStore::get().load.manualMode && thermo_ >= cfg.turnOffC) {
          if (loadController_->pwmRaw() > 0 || loadController_->isAutomaticMode()) {
            loadController_->setRunning(false);
            loadController_->shutdown(false, "Corte por temperatura (modo automatico)\n");
          }
        }
        break;
      case 2: // Manual
        if (manualActive && thermo_ < cfg.turnOnC) { loadController_->setRunning(true); }
        if (manualActive && thermo_ >= cfg.turnOffC) {
          if (loadController_->pwmRaw() > 0) {
            loadController_->setRunning(false);
            loadController_->shutdown(false, "Corte por temperatura (modo manual)\n");
          }
        }
        break;
      case 3: // Automático + Manual
        if (thermo_ < cfg.turnOnC) { loadController_->setRunning(true); }
        if (thermo_ >= cfg.turnOffC) {
          if (loadController_->pwmRaw() > 0 || loadController_->isAutomaticMode()) {
            loadController_->setRunning(false);
            loadController_->shutdown(false, "Corte por temperatura (auto+manual)\n");
          }
        }
        break;
      default:
        break;
    }
  }

  if ((millis() - lastThermoOkMs_) > maxErrorMs && cfg.mode > 0 && cfg.thermoSensorAddr[0] != 0x0) {
    if (loadController_->pwmRaw() > 0) {
      loadController_->setRunning(false);
      loadController_->shutdown(false, "Corte por falha na leitura da temperatura\n");
    }
  }
}

void TemperatureManager::loop() {
  if (!ConfigStore::get().temperature.enabled) return;
  readSensors();
  applyControl();
}
