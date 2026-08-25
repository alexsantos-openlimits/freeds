/*
  Types.h - Tipos de dados partilhados pela aplicação Lusol
*/
#pragma once

#include <Arduino.h>

// Leitura unificada de energia produzida por qualquer gestor de excedentes
// (contador de rede, inversor, MQTT, etc). Cada gestor preenche apenas os
// campos que a sua fonte de dados suporta; os restantes ficam a 0.
struct PowerReading {
  float gridWatts = 0;          // + consumo da rede / - injeção na rede (conforme changeGridSign)
  float solarWatts = 0;         // Potência solar instantânea
  float solarWattsToday = 0;    // Energia solar produzida hoje (Wh, conforme fonte)
  float gridVoltage = 0;
  float gridCurrent = 0;
  float frequency = 0;
  float powerFactor = 0;
  float importActiveEnergy = 0;
  float exportActiveEnergy = 0;
  float pv1Current = 0;
  float pv2Current = 0;
  float pv1Voltage = 0;
  float pv2Voltage = 0;
  float pv1Watts = 0;
  float pv2Watts = 0;
  float inverterTemperature = 0;
  float batteryWatts = 0;       // + a carregar / - a descarregar
  float batterySoc = 0;         // %
  float loadWatts = 0;
};

// Campos que uma leitura efetivamente contém, usado pela interface web para
// decidir o que mostrar. Substitui o antigo bitfield "webMonitorFields".
struct PowerReadingFields {
  bool gridWatts = false;
  bool solarWatts = false;
  bool solarWattsToday = false;
  bool gridVoltage = false;
  bool gridCurrent = false;
  bool frequency = false;
  bool powerFactor = false;
  bool importActiveEnergy = false;
  bool exportActiveEnergy = false;
  bool pv1 = false;
  bool pv2 = false;
  bool inverterTemperature = false;
  bool batteryWatts = false;
  bool batterySoc = false;
  bool loadWatts = false;
};

enum class LoadWorkingMode : uint8_t {
  Auto = 0,
  Manual = 1,
  Off = 2,
};

// Estado de uma saída de relé (carga escalonada). Substitui as 4 cópias
// quase idênticas de código que existiam por saída no firmware original.
struct RelayOutputConfig {
  uint16_t minPercent = 999;  // 999 = desativado (controlo por percentagem)
  int16_t onWatts = 9999;     // 9999 = desativado (controlo por potência)
  int16_t offWatts = 9999;
  bool manual = false;        // Forçado manualmente pelo utilizador
};

struct RelayOutputState {
  bool energized = false;
  bool autoActivated = false;
};
