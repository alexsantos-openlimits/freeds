/*
  MqttSurplusManager.h - Gestor de excedentes alimentado por MQTT

  Substitui a parte de leitura de dados de mqtt.ino (onMqttMessage() para os
  modos MQTT_BROKER e ICC_SOLAR, suscribeMqttMeter()). A parte de ligação ao
  broker, publicação de estado e comandos (relés, pwm, domoticz, ...) já não
  é responsabilidade deste gestor - fica em MqttService.

  Cobre dois modos de funcionamento distintos, mutuamente exclusivos tal
  como no código original:

  - MQTT_BROKER (41): a própria app do inversor/contador (ex.: plugin
    Tasmota-Solax) publica JSON estilo Tasmota nos tópicos configuráveis
    mqtt.solaxTopic (dados do inversor) e mqtt.meterTopic (dados de rede).

  - ICC_SOLAR (42): o sistema "ICC Solar" publica valores simples (não
    JSON) num conjunto fixo de tópicos "Inverter/...", mais o tópico
    configurável mqtt.socTopic para o estado de carga da bateria.

  Cada tópico atualiza apenas os campos de PowerReading que lhe
  correspondem; os restantes mantêm o último valor conhecido, tal como o
  original (que ia combinando dados de várias mensagens ao longo do tempo).

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include "SurplusManager.h"

class MqttSurplusManager : public SurplusManagerBase {
public:
  MqttSurplusManager();

  void begin() override;
  void loop() override;
  const char *name() const override { return "MQTT"; }

private:
  // Modo MQTT_BROKER: JSON estilo Tasmota.
  void handleSolaxTopic(const char *topic, const char *payload);
  void handleMeterTopic(const char *topic, const char *payload);

  // Modo ICC_SOLAR: valores simples (atof) nos tópicos fixos "Inverter/..."
  // e no tópico configurável do SoC.
  void handleIccTopic(const char *topic, const char *payload);
};
