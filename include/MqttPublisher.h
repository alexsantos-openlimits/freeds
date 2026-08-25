/*
  MqttPublisher.h - Publicação periódica de estado no MQTT

  Substitui a parte de publish() que existia em mqtt.ino (estado dos relés
  por tópico individual + republicação periódica). Na reescrita anterior o
  MqttService só tratava da ligação, mas nada chamava publish() - por isso
  o MQTT "não fazia nada" mesmo quando a ligação ao broker funcionava.

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include <Arduino.h>
#include "LoadController.h"
#include "SurplusManager.h"
#include "CurrentClampSensor.h"

class MqttPublisher {
public:
  void begin(LoadController *load, SurplusManager **surplus, CurrentClampSensor *clamp);
  void loop();

private:
  void publishStatusJson();

  LoadController *load_ = nullptr;
  SurplusManager **surplus_ = nullptr;
  CurrentClampSensor *clamp_ = nullptr;

  bool lastRelayOn_[4] = {false, false, false, false};
  bool relayStatePublished_[4] = {false, false, false, false};
  unsigned long lastStatusPublishMs_ = 0;
  char statusTopic_[48] = {0};
};
