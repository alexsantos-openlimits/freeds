/*
  FreeDS.ino - Ponto de entrada da aplicação (setup/loop)
  Gestor de excedentes para ESP32 (Heltec WiFi Kit 32)

  Esta reescrita mantém o hardware original (mesmos pinos, mesmo ecrã OLED,
  mesmas 4 saídas de relé, mesma saída PWM/DAC, mesmo sensor de corrente e
  mesmo barramento de sensores de temperatura) mas substitui o monólito de
  variáveis globais e ficheiros .ino concatenados por uma arquitetura de
  classes (ver include/*.h e src/core/*.cpp): ConfigStore, LoadController,
  SurplusManager (com uma implementação por protocolo em src/managers/),
  NetworkManager, MqttService, TemperatureManager, DisplayManager,
  EnergyTracker e WebApi.

  Inspirado em opends+ (https://github.com/iqas/derivador)

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <Arduino.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <SSD1306.h>

extern "C" {
#include <driver/dac.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>
}

#include "AppConfig.h"
#include "Logger.h"
#include "Types.h"
#include "SurplusManager.h"
#include "LoadController.h"
#include "NetworkManager.h"
#include "MqttService.h"
#include "TemperatureManager.h"
#include "DisplayManager.h"
#include "EnergyTracker.h"
#include "CurrentClampSensor.h"
#include "WebApi.h"
#include "workingmode.h"
#include "managers/HttpInverterManager.h"

// ----------------------------------------------------------------------
// Pinagem de hardware (inalterada face à placa FreeDS original)
// ----------------------------------------------------------------------
static const uint8_t PIN_PWM = 25;
static const uint8_t PIN_ESP01_RX = 17, PIN_ESP01_TX = 5; // UART2 (módulo ESP-01, modo Solax V2)
static const uint8_t PIN_METER_RX = 19, PIN_METER_TX = 23; // UART1 (contador Modbus RTU)
static const uint8_t PIN_RELAY[4] = {13, 12, 14, 27};
static const uint8_t PIN_ADC_CLAMP = 34;
static const uint8_t PIN_DS18B20 = 2;
static const uint8_t PIN_BUTTON = 0;
static const uint8_t PIN_OLED_RESET = 16;

SSD1306 g_display(0x3c, 4, 15);

LoadController g_loadController;
NetworkManager g_networkManager;
TemperatureManager g_temperatureManager;
DisplayManager g_displayManager;
EnergyTracker g_energyTracker;
CurrentClampSensor g_clampSensor;
WebApi g_webApi;

SurplusManager *g_surplusManager = nullptr;

// Implementada em src/managers/SurplusManagerFactory.cpp, depois de todas as
// implementações de gestores de excedentes estarem prontas.
SurplusManager *createSurplusManager();

hw_timer_t *g_watchdogTimer = nullptr;

void IRAM_ATTR resetModule() { ESP.restart(); }

struct ButtonState {
  bool pressed = false;
  bool longPressHandled = false;
  unsigned long pressStartMs = 0;
} g_button;

void recreateSurplusManager() {
  delete g_surplusManager;
  g_surplusManager = createSurplusManager();
  if (g_surplusManager) g_surplusManager->begin();
}

void handleButton() {
  bool down = digitalRead(PIN_BUTTON) == LOW;

  if (down) {
    if (!g_button.pressed) {
      g_button.pressed = true;
      g_button.pressStartMs = millis();
    }
    unsigned long held = millis() - g_button.pressStartMs;

    if (held > 10000) {
      ConfigStore::resetToDefaults();
      ESP.restart();
    } else if (held > 2000 && !g_button.longPressHandled) {
      g_button.longPressHandled = true;
      g_loadController.toggleWorkingMode();
    }
  } else if (g_button.pressed) {
    if (!g_button.longPressHandled) {
      g_displayManager.nextScreen();
    }
    g_button.pressed = false;
    g_button.longPressHandled = false;
  }
}

void setup() {
  // Watchdog: reinicia se o loop principal ficar bloqueado mais de 30s.
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  delay(1000);
  g_watchdogTimer = timerBegin(0, 240, true);
  timerAttachInterrupt(g_watchdogTimer, &resetModule, true);
  timerAlarmWrite(g_watchdogTimer, 30000000, false);
  timerAlarmEnable(g_watchdogTimer);

  // Monitor de corrente (pinça amperimétrica opcional)
  adc1_config_width(ADC_WIDTH_BIT_12);
  adc1_config_channel_atten(ADC1_CHANNEL_6, ADC_ATTEN_DB_11);

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  if (!SPIFFS.begin(true)) {
    Serial.println("Erro ao montar o sistema de ficheiros (SPIFFS)");
  }

  ConfigStore::begin();

  pinMode(PIN_OLED_RESET, OUTPUT);
  digitalWrite(PIN_OLED_RESET, LOW);
  delay(50);
  digitalWrite(PIN_OLED_RESET, HIGH);

  g_display.init();
  g_display.setBrightness(ConfigStore::get().system.oledBrightness);
  if (ConfigStore::get().system.flipScreen) g_display.flipScreenVertically();

  g_displayManager.begin(&g_display, &g_loadController, &g_temperatureManager);
  g_displayManager.showLogo("Iniciando...", true);

  LoadControllerPins pins;
  pins.pwmPin = PIN_PWM;
  pins.dacChannel = DAC_CHANNEL_2;
  pins.ledcChannel = 2;
  for (uint8_t i = 0; i < 4; i++) pins.relayPins[i] = PIN_RELAY[i];
  g_loadController.begin(pins);

  g_temperatureManager.begin(PIN_DS18B20, &g_loadController);
  g_energyTracker.begin();
  g_clampSensor.begin(PIN_ADC_CLAMP);

  g_networkManager.begin();

  // A mesma interface web nova serve tanto o modo "ponto de acesso" inicial
  // (para configurar o Wi-Fi) como o funcionamento normal - substitui o
  // antigo portal cativo HTML à parte.
  g_webApi.begin(&g_loadController, &g_temperatureManager, &g_networkManager, &g_surplusManager,
                 &g_clampSensor, recreateSurplusManager);

  if (g_networkManager.isAccessPointMode()) {
    g_displayManager.showLogo("LIGUE-SE AO SSID:\nFreeDS\n192.168.4.1", false);
  } else {
    MqttService::begin();
    recreateSurplusManager();
    g_displayManager.showLogo(WiFi.localIP().toString(), true);
  }
}

unsigned long g_lastSlowLoopMs = 0;

void loop() {
  timerWrite(g_watchdogTimer, 0); // alimenta o watchdog

  g_networkManager.loop();

  if (g_networkManager.isAccessPointMode()) {
    return; // só o portal de configuração está ativo
  }

  MqttService::loop();
  g_webApi.loop();

  if (g_surplusManager) {
    g_surplusManager->loop();
    g_loadController.onNewReading(g_surplusManager->reading(), g_surplusManager->isConnected());

    // Modo escravo: outro FreeDS "mestre" pode pedir para desligar este PWM
    // (ex.: o mestre parou por temperatura ou já não tem excedente).
    if (ConfigStore::get().surplus.mode == SLAVE_MODE) {
      auto *slave = static_cast<HttpInverterManager *>(g_surplusManager);
      g_loadController.setRunning(!slave->masterRequestsPwmDisable());
    }
  }

  g_loadController.update();
  g_clampSensor.loop(g_loadController.pwmPercent());
  handleButton();
  g_displayManager.update(g_surplusManager, const_cast<uint8_t *>(PIN_RELAY));

  // Tarefas de baixa frequência (temperatura, contabilização de energia, hora)
  if (millis() - g_lastSlowLoopMs > 1000) {
    g_lastSlowLoopMs = millis();

    g_temperatureManager.loop();
    g_networkManager.updateLocalTime();

    struct tm now;
    bool hasTime = getLocalTime(&now);
    float gridWatts = g_surplusManager ? g_surplusManager->reading().gridWatts : 0;
    g_energyTracker.update(gridWatts, hasTime && g_networkManager.hasNtpTime(), now);
  }
}
