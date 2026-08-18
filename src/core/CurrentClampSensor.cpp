#include "CurrentClampSensor.h"
#include "AppConfig.h"

namespace {
const unsigned int ADC_COUNTS = 4096; // ADC de 12 bits
}

void CurrentClampSensor::begin(uint8_t adcPin) {
  pin_ = adcPin;
  offsetI_ = ADC_COUNTS >> 1;
}

double CurrentClampSensor::readIrms(unsigned int numberOfSamples) {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
  const uint16_t supplyMillivolts = 3300;

  double sampleI, filteredI, sqI, sumI = 0;

  for (unsigned int n = 0; n < numberOfSamples; n++) {
    uint16_t reading = analogRead(pin_);
    if (reading < 1 || reading > 4095) reading = 0;

    // Polinómio de calibração específico do sensor SCT-013 + resistência de
    // carga usados na placa FreeDS original - copiado tal e qual, não é um
    // valor arbitrário para "melhorar".
    sampleI = -0.000000000000016 * pow((double)reading, 4) + 0.000000000118171 * pow((double)reading, 3) -
              0.000000301211691 * pow((double)reading, 2) + 0.001109019271794 * (double)reading + 0.034143524634089;

    offsetI_ = offsetI_ + (sampleI - offsetI_) / ADC_COUNTS;
    filteredI = sampleI - offsetI_;

    sqI = filteredI * filteredI;
    sumI += sqI;
  }

  double iRatio = (cfg.clampCalibration * 1000) * ((supplyMillivolts / 1000.0) / ADC_COUNTS);
  double irms = iRatio * sqrt(sumI / numberOfSamples);

  return irms;
}

void CurrentClampSensor::loop(uint8_t pwmPercent) {
  if (millis() - lastReadMs_ < 500) return;
  lastReadMs_ = millis();

  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  if (cfg.useClamp) {
    double amps = readIrms(1484); // ~130ms, tal como o original
    lastWatts_ = amps > 0.50 ? (float)(amps * cfg.clampVoltage) : 0.0f;
  } else {
    // Curva de potência de um dimmer de fase (triac): P = sin²(ângulo) * Pmax
    lastWatts_ = (float)(sq(sin((pwmPercent / 100.0) * (PI / 2.0))) * cfg.attachedLoadWatts);
  }
}
