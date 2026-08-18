#include "DisplayManager.h"
#include "LoadController.h"
#include "TemperatureManager.h"
#include "SurplusManager.h"
#include "Logger.h"

#include <SSD1306.h>
#include <WiFi.h>
#include <bitmap.h>

extern int WifiGetRssiAsQuality(int rssi); // NetworkManager.cpp

namespace {
// Pequeno dicionário embutido (só pt/en) para os rótulos fixos do ecrã OLED.
// A interface web tem o seu próprio i18n completo em data/i18n/*.json; aqui
// mantemos algo mínimo por ser um ecrã de 128x64px com pouco texto.
const char *L(const char *pt, const char *en) {
  return strcmp(ConfigStore::get().system.language, "en") == 0 ? en : pt;
}
} // namespace

void DisplayManager::begin(SSD1306 *display, LoadController *loadController, TemperatureManager *temperature) {
  display_ = display;
  loadController_ = loadController;
  temperature_ = temperature;
}

void DisplayManager::showLogo(const String &text, bool blocking) {
  display_->clear();
  display_->drawFastImage(0, 0, 128, 64, FreeDS);
  display_->setTextAlignment(TEXT_ALIGN_CENTER);
  display_->setFont(ArialMT_Plain_10);
  int8_t nl = text.indexOf('\n');
  if (nl == -1) {
    display_->drawString(87, 45, text);
  } else {
    display_->drawString(87, 40, text.substring(0, nl));
    display_->drawString(87, 50, text.substring(nl + 1));
  }
  display_->display();
  if (blocking) delay(2000);
}

void DisplayManager::nextScreen() {
  screen_ = (screen_ + 1) % (MAX_SCREENS + 1);
}

void DisplayManager::setPower(bool on) {
  display_->clear();
  on ? display_->displayOn() : display_->displayOff();
}

void DisplayManager::update(const SurplusManager *surplus, uint8_t relayPins[4]) {
  if (!surplus) return; // ainda sem gestor de excedentes ativo (ex.: modo ponto de acesso)

  if (millis() - lastFlashMs_ > 1000) {
    lastFlashMs_ = millis();
    flash_ = !flash_;
  }

  switch (screen_) {
    case 0: drawHome(surplus, relayPins); break;
    case 1: drawStrings(surplus); break;
    case 2: drawMeter(surplus); break;
    case 3: drawWifiInfo(); break;
    case 4: drawTemperatures(); break;
    case 5: drawBuildInfo(); break;
  }
}

void DisplayManager::drawHome(const SurplusManager *surplus, uint8_t relayPins[4]) {
  const PowerReading &r = surplus->reading();
  const PowerReadingFields &f = surplus->fields();
  bool showsBattery = f.batteryWatts && !f.solarWatts;

  display_->clear();
  display_->setFont(ArialMT_Plain_10);

  display_->setTextAlignment(TEXT_ALIGN_LEFT);
  display_->drawString(0, 0, showsBattery ? L("Bateria", "Battery") : L("Solar", "Solar"));
  display_->setTextAlignment(TEXT_ALIGN_RIGHT);
  display_->drawString(128, 0, f.batterySoc && !f.gridVoltage ? "SoC" : L("Rede", "Grid"));

  display_->setFont(ArialMT_Plain_24);
  display_->setTextAlignment(TEXT_ALIGN_LEFT);
  display_->drawString(0, 12, String((int)(showsBattery ? r.batteryWatts : r.solarWatts)));
  display_->setTextAlignment(TEXT_ALIGN_RIGHT);
  if (f.batterySoc && !f.gridVoltage) {
    display_->drawString(128, 12, String((int)r.batterySoc) + "%");
  } else {
    display_->drawString(128, 12, String((int)r.gridWatts));
  }

  display_->setFont(ArialMT_Plain_10);
  display_->setTextAlignment(TEXT_ALIGN_CENTER);
  display_->drawString(69, 0, surplus->name());

  bool sourceFault = !surplus->isConnected() || loadController_->hasDataFault();

  if (sourceFault && loadController_->pwmRaw() <= 1) {
    display_->drawString(64, 38, WiFi.localIP().toString());
  } else {
    display_->drawProgressBar(0, 38, 127, 12, loadController_->pwmPercent());
    display_->setTextAlignment(TEXT_ALIGN_CENTER);
    if (loadController_->workingMode() == LoadWorkingMode::Off) {
      display_->drawString(64, 38, "PWM: OFF");
    } else {
      display_->setColor(INVERSE);
      String modeLabel = loadController_->workingMode() == LoadWorkingMode::Manual ? " (MANUAL)" : "";
      display_->drawString(64, 38, "PWM: " + String(loadController_->pwmPercent()) + "%" + modeLabel);
      display_->setColor(WHITE);
    }
  }

  display_->setTextAlignment(TEXT_ALIGN_LEFT);
  display_->drawString(5, 52, flash_ ? "S" : (sourceFault ? "_" : "S"));
  display_->drawString(17, 52, flash_ ? "W" : (WiFi.status() != WL_CONNECTED ? "_" : "W"));
  display_->drawString(30, 52, flash_ ? "M" : "M");

  display_->setTextAlignment(TEXT_ALIGN_RIGHT);
  String relays = L("Saidas:", "Outputs:");
  for (uint8_t i = 0; i < 4; i++) relays += digitalRead(relayPins[i]) ? (String(i + 1) + " ") : "_ ";
  display_->drawString(128, 52, relays);

  display_->display();
}

void DisplayManager::drawStrings(const SurplusManager *surplus) {
  const PowerReading &r = surplus->reading();
  const PowerReadingFields &f = surplus->fields();
  if (!f.pv1 && !f.pv2) { nextScreen(); return; }

  display_->clear();
  display_->setFont(ArialMT_Plain_10);
  display_->setTextAlignment(TEXT_ALIGN_CENTER);
  display_->drawString(64, 0, L("Dados do inversor", "Inverter data"));
  display_->drawString(19, 12, L("Potencia", "Power"));
  display_->drawString(60, 12, L("Rede", "Grid"));
  display_->drawString(102, 12, L("Hoje", "Today"));
  display_->drawString(19, 22, String((int)r.solarWatts) + "W");
  display_->drawString(60, 22, String((int)r.gridWatts) + "W");
  display_->drawString(102, 22, String(r.solarWattsToday) + "Kw");
  display_->drawString(30, 34, "STRING 1");
  display_->drawString(30, 44, String((int)r.pv1Watts) + "W");
  display_->drawString(30, 54, String((int)r.pv1Voltage) + "V " + String(r.pv1Current) + "A");
  display_->drawString(100, 34, "STRING 2");
  display_->drawString(100, 44, String((int)r.pv2Watts) + "W");
  display_->drawString(100, 54, String((int)r.pv2Voltage) + "V " + String(r.pv2Current) + "A");
  display_->display();
}

void DisplayManager::drawMeter(const SurplusManager *surplus) {
  const PowerReading &r = surplus->reading();
  const PowerReadingFields &f = surplus->fields();
  if (!f.gridVoltage || !f.gridCurrent) { nextScreen(); return; }

  display_->clear();
  display_->setFont(ArialMT_Plain_10);
  display_->setTextAlignment(TEXT_ALIGN_CENTER);
  display_->drawString(64, 0, L("Dados do medidor", "Meter data"));
  display_->drawString(19, 12, L("Potencia", "Power"));
  display_->drawString(60, 12, L("Voltagem", "Voltage"));
  display_->drawString(102, 12, L("Intensidade", "Current"));
  display_->drawString(19, 22, String((int)r.gridWatts) + "W");
  display_->drawString(60, 22, String((int)r.gridVoltage) + "V");
  display_->drawString(102, 22, String(r.gridCurrent) + "A");
  display_->drawString(30, 34, L("CONSUMIDO", "IMPORTED"));
  display_->drawString(30, 44, String(r.importActiveEnergy) + "KWH");
  display_->drawString(100, 34, L("ENVIADO", "EXPORTED"));
  display_->drawString(100, 44, String(r.exportActiveEnergy) + "KWH");
  display_->display();
}

void DisplayManager::drawWifiInfo() {
  display_->clear();
  display_->setFont(ArialMT_Plain_10);
  display_->setTextAlignment(TEXT_ALIGN_LEFT);
  display_->drawString(0, 0, "IP: " + WiFi.localIP().toString());
  display_->drawString(0, 12, "SSID: " + WiFi.SSID() + " (" + String(WifiGetRssiAsQuality((int8_t)WiFi.RSSI())) + "%)");
  display_->drawString(0, 24, String(L("Frequencia PWM: ", "PWM frequency: ")) + String((float)ConfigStore::get().load.pwmFrequencyHz / 1000) + "Khz");
  display_->drawString(0, 36, "PWM: " + String(loadController_->pwmPercent()) + "% (" + String(loadController_->pwmRaw()) + ")");
  display_->drawString(0, 48, String(L("Duracao: ", "Uptime: ")) + String(millis() / 1000) + "s");
  display_->display();
}

void DisplayManager::drawTemperatures() {
  display_->clear();
  display_->setFont(ArialMT_Plain_10);
  display_->setTextAlignment(TEXT_ALIGN_CENTER);
  display_->drawString(64, 0, L("Temperaturas", "Temperatures"));
  display_->setTextAlignment(TEXT_ALIGN_LEFT);
  display_->drawString(0, 12, String(L("Temp. Termo: ", "Tank temp.: ")) + String(temperature_->thermoTemperature()) + "C");
  display_->drawString(0, 24, String(L("Temp. Triac: ", "Triac temp.: ")) + String(temperature_->triacTemperature()) + "C");
  display_->drawString(0, 36, String(ConfigStore::get().temperature.customSensorName) + ": " + String(temperature_->customTemperature()) + "C");
  display_->display();
}

void DisplayManager::drawBuildInfo() {
  display_->clear();
  display_->setFont(ArialMT_Plain_24);
  display_->setTextAlignment(TEXT_ALIGN_CENTER);
  display_->drawString(64, 0, "FreeDS");
  display_->setFont(ArialMT_Plain_10);
  display_->drawString(64, 25, L("Gestor de excedentes", "Surplus manager"));
  display_->drawString(64, 40, L("Compilacao:", "Build:"));
  display_->drawString(64, 50, "(" __DATE__ " " __TIME__ ")");
  display_->display();
}
