/*
  DisplayManager.h - Ecrã OLED (SSD1306)

  Substitui display.ino. Em vez de decidir o que mostrar a partir do
  identificador do modo de gestor de excedentes (switch/case por protocolo),
  usa agora PowerReadingFields para saber que dados a fonte atual
  disponibiliza — o ecrã adapta-se automaticamente a qualquer gestor,
  incluindo os que vierem a ser adicionados no futuro.

  Hardware inalterado: SSD1306 I2C (endereço 0x3c, SDA=4, SCL=15), pino de
  reset 16, tal como na placa FreeDS original.

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include <Arduino.h>
#include "AppConfig.h"
#include "Types.h"

class SSD1306;
class LoadController;
class TemperatureManager;
class SurplusManager;

class DisplayManager {
public:
  void begin(SSD1306 *display, LoadController *loadController, TemperatureManager *temperature);

  void showLogo(const String &text, bool blocking);
  void update(const SurplusManager *surplus, uint8_t relayPins[4]);

  void nextScreen();
  void setPower(bool on);

private:
  void drawHome(const SurplusManager *surplus, uint8_t relayPins[4]);
  void drawStrings(const SurplusManager *surplus);
  void drawMeter(const SurplusManager *surplus);
  void drawWifiInfo();
  void drawTemperatures();
  void drawBuildInfo();

  SSD1306 *display_ = nullptr;
  LoadController *loadController_ = nullptr;
  TemperatureManager *temperature_ = nullptr;

  uint8_t screen_ = 0;
  static const uint8_t MAX_SCREENS = 5;
  bool flash_ = false;
  unsigned long lastFlashMs_ = 0;
};
