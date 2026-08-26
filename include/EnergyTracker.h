/*
  EnergyTracker.h - Contabilização de energia importada/exportada (kWh)

  Substitui calcWattsToday() em Support_functions.ino.
*/
#pragma once

#include <Arduino.h>
#include <time.h>

class EnergyTracker {
public:
  void begin() { lastMs_ = millis(); }
  void update(float gridWatts, bool sourceConnected, bool ntpTimeValid, const struct tm &now);

private:
  unsigned long lastMs_ = 0;
};
