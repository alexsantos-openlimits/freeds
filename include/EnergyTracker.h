/*
  EnergyTracker.h - Contabilização de energia importada/exportada (kWh)

  Substitui calcWattsToday() em Support_functions.ino.

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include <Arduino.h>
#include <time.h>

class EnergyTracker {
public:
  void begin() { lastMs_ = millis(); }
  void update(float gridWatts, bool ntpTimeValid, const struct tm &now);

private:
  unsigned long lastMs_ = 0;
};
