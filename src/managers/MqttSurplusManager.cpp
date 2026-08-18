/*
  MqttSurplusManager.cpp - ver MqttSurplusManager.h

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#include "managers/MqttSurplusManager.h"

#include <ArduinoJson.h>
#include <cstdlib>
#include <cstring>

#include "AppConfig.h"
#include "Logger.h"
#include "MqttService.h"
#include "workingmode.h"

MqttSurplusManager::MqttSurplusManager() {}

void MqttSurplusManager::begin() {
  const MqttConfig &mqtt = ConfigStore::get().mqtt;
  uint8_t mode = ConfigStore::get().surplus.mode;

  if (mode == ICC_SOLAR) {
    // Tópicos fixos publicados pelo sistema ICC Solar (ver suscribeMqttMeter()
    // no mqtt.ino original).
    static const char *iccTopics[] = {
        "Inverter/GridWatts",
        "Inverter/MPPT1_Watts",
        "Inverter/MPPT2_Watts",
        "Inverter/MPPT1_Volts",
        "Inverter/MPPT2_Volts",
        "Inverter/MPPT1_Amps",
        "Inverter/MPPT2_Amps",
        "Inverter/PvWattsTotal",
        "Inverter/SolarKwUse",
        "Inverter/BatteryVolts",
        "Inverter/BatteryAmps",
        "Inverter/BatteryWatts",
        "Inverter/LoadWatts",
        "Inverter/Temperature",
    };

    for (const char *topic : iccTopics) {
      MqttService::onTopic(topic, [this](const char *t, const char *payload) { handleIccTopic(t, payload); });
    }
    // Tópico configurável para o estado de carga (SoC) da bateria.
    MqttService::onTopic(mqtt.socTopic, [this](const char *t, const char *payload) { handleIccTopic(t, payload); });

    Logger::info("MqttSurplusManager: modo ICC Solar, a subscrever topicos Inverter/* e %s\n", mqtt.socTopic);
    return;
  }

  // MQTT_BROKER (e valor por omissão): dados JSON estilo Tasmota nos
  // tópicos configuráveis solaxTopic (inversor) e meterTopic (rede).
  MqttService::onTopic(mqtt.solaxTopic, [this](const char *t, const char *payload) { handleSolaxTopic(t, payload); });
  MqttService::onTopic(mqtt.meterTopic, [this](const char *t, const char *payload) { handleMeterTopic(t, payload); });

  Logger::info("MqttSurplusManager: modo MQTT Broker, a subscrever %s e %s\n", mqtt.solaxTopic, mqtt.meterTopic);
}

void MqttSurplusManager::loop() {
  // Toda a atualização acontece de forma assíncrona nos handlers registados
  // via MqttService::onTopic() (chamados a partir de MqttService::loop() /
  // AsyncMqttClient); não há nada a fazer periodicamente aqui.
}

void MqttSurplusManager::handleMeterTopic(const char *topic, const char *payload) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Logger::info("MqttSurplusManager: deserializeJson() Meter MQTT falhou: %s\n", error.c_str());
    return;
  }

  PowerReading r = reading_;
  // Potencia de red (Negativo: de red - Positivo: a red) para usar con los
  // datos de Tasmota.
  r.gridWatts = (float)doc["ENERGY"]["Power"];
  r.gridVoltage = (float)doc["ENERGY"]["Voltage"];

  if (!ConfigStore::get().surplus.changeGridSign) {
    r.gridWatts *= -1.0f;
  }

  fields_.gridWatts = true;
  fields_.gridVoltage = true;

  publishReading(r);
}

void MqttSurplusManager::handleSolaxTopic(const char *topic, const char *payload) {
  DynamicJsonDocument doc(512);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Logger::info("MqttSurplusManager: deserializeJson() Solax MQTT falhou: %s\n", error.c_str());
    return;
  }

  PowerReading r = reading_;
  r.pv1Current = (float)doc["ENERGY"]["Pv1Current"];
  r.pv2Current = (float)doc["ENERGY"]["Pv2Current"];
  r.pv1Voltage = (float)doc["ENERGY"]["Pv1Voltage"];
  r.pv2Voltage = (float)doc["ENERGY"]["Pv2Voltage"];
  r.pv1Watts = (float)doc["ENERGY"]["Pv1Power"];
  r.pv2Watts = (float)doc["ENERGY"]["Pv2Power"];
  r.solarWattsToday = (float)doc["ENERGY"]["Today"];
  r.solarWatts = (float)doc["ENERGY"]["Power"];
  r.inverterTemperature = (float)doc["ENERGY"]["Temperature"];

  fields_.solarWatts = true;
  fields_.solarWattsToday = true;
  fields_.pv1 = true;
  fields_.pv2 = true;
  fields_.inverterTemperature = true;

  publishReading(r);
}

void MqttSurplusManager::handleIccTopic(const char *topic, const char *payload) {
  const MqttConfig &mqtt = ConfigStore::get().mqtt;
  PowerReading r = reading_;
  float value = atof(payload);

  if (strcmp(topic, "Inverter/GridWatts") == 0) {
    if (!ConfigStore::get().surplus.changeGridSign) { value *= -1.0f; }
    r.gridWatts = value;
    fields_.gridWatts = true;
  } else if (strcmp(topic, "Inverter/MPPT1_Watts") == 0) {
    r.pv1Watts = value;
    fields_.pv1 = true;
  } else if (strcmp(topic, "Inverter/MPPT2_Watts") == 0) {
    r.pv2Watts = value;
    fields_.pv2 = true;
  } else if (strcmp(topic, "Inverter/MPPT1_Volts") == 0) {
    r.pv1Voltage = value;
    fields_.pv1 = true;
  } else if (strcmp(topic, "Inverter/MPPT2_Volts") == 0) {
    r.pv2Voltage = value;
    fields_.pv2 = true;
  } else if (strcmp(topic, "Inverter/MPPT1_Amps") == 0) {
    r.pv1Current = value;
    fields_.pv1 = true;
  } else if (strcmp(topic, "Inverter/MPPT2_Amps") == 0) {
    r.pv2Current = value;
    fields_.pv2 = true;
  } else if (strcmp(topic, "Inverter/PvWattsTotal") == 0) {
    r.solarWatts = value;
    fields_.solarWatts = true;
  } else if (strcmp(topic, "Inverter/SolarKwUse") == 0) {
    r.solarWattsToday = value;
    fields_.solarWattsToday = true;
  } else if (strcmp(topic, "Inverter/BatteryVolts") == 0) {
    // No original guarda-se em meter.voltage só para o mostrador; não
    // existe campo próprio de tensão de bateria em PowerReading. Ainda
    // assim a mensagem é considerada "recebida com sucesso" (ver abaixo).
  } else if (strcmp(topic, "Inverter/BatteryAmps") == 0) {
    // Idem, para meter.current.
  } else if (strcmp(topic, "Inverter/BatteryWatts") == 0) {
    r.batteryWatts = value;
    fields_.batteryWatts = true;
  } else if (strcmp(topic, "Inverter/LoadWatts") == 0) {
    r.loadWatts = value;
    fields_.loadWatts = true;
  } else if (strcmp(topic, "Inverter/Temperature") == 0) {
    r.inverterTemperature = value;
    fields_.inverterTemperature = true;
  } else if (strcmp(topic, mqtt.socTopic) == 0) {
    r.batterySoc = value;
    fields_.batterySoc = true;
  } else {
    return;
  }

  publishReading(r);
}
