#include "EnergyTracker.h"
#include "AppConfig.h"

void EnergyTracker::update(float gridWatts, bool ntpTimeValid, const struct tm &now) {
  if (!ntpTimeValid) {
    lastMs_ = millis();
    return;
  }

  // Descarta leituras absurdas (evita contaminar os totais com ruído/erros de leitura)
  if (gridWatts < -20000 || gridWatts > 20000) {
    lastMs_ = millis();
    return;
  }

  EnergyTotals &energy = ConfigStore::get().energy;
  bool changeSign = ConfigStore::get().surplus.changeGridSign;

  float dtHours = (millis() - lastMs_) / 1000.0f / 3600.0f;
  float kwIncrement = gridWatts * dtHours / 1000.0f;

  if (changeSign ? kwIncrement > 0 : kwIncrement < 0) {
    if (!changeSign) kwIncrement *= -1;
    energy.kwToday += kwIncrement;
    energy.kwTotal += kwIncrement;
  } else {
    if (changeSign) kwIncrement *= -1;
    energy.kwExportToday += kwIncrement;
    energy.kwExportTotal += kwIncrement;
  }

  if (now.tm_hour == 0 && now.tm_min == 0 && now.tm_sec == 0) {
    energy.kwYesterday = energy.kwToday;
    energy.kwExportYesterday = energy.kwExportToday;
    energy.kwToday = 0;
    energy.kwExportToday = 0;
    ConfigStore::save();
  }

  lastMs_ = millis();
}
