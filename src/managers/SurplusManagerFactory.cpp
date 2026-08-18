/*
  SurplusManagerFactory.cpp - Cria a implementação de SurplusManager
  correspondente ao modo configurado (ConfigStore::get().surplus.mode)

  Substitui o switch/case espalhado por getSensorData()/setGetDataTime()/
  defineWebMonitorFields() no firmware original por um único ponto de
  decisão. Chamado no arranque e sempre que o utilizador grava uma nova
  configuração de gestor de excedentes (ver WebApi::onSurplusModeChanged).

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#include "AppConfig.h"
#include "SurplusManager.h"
#include "workingmode.h"

#include "managers/ModbusRtuMeterManager.h"
#include "managers/ModbusTcpManager.h"
#include "managers/HttpInverterManager.h"
#include "managers/MqttSurplusManager.h"
#include "managers/GoodweManager.h"

SurplusManager *createSurplusManager() {
  uint8_t mode = ConfigStore::get().surplus.mode;

  switch (mode) {
    case DDS238_METER:
    case DDSU666_METER:
    case SDM_METER:
    case MUSTSOLAR:
      return new ModbusRtuMeterManager();

    case SOLAX_V2:
    case SOLAX_V2_LOCAL:
    case SOLAX_V1:
    case WIBEEE:
    case SHELLY_EM:
    case FRONIUS_API:
    case SLAVE_MODE:
      return new HttpInverterManager();

    case GOODWE:
      return new GoodweManager();

    case MQTT_BROKER:
    case ICC_SOLAR:
      return new MqttSurplusManager();

    case SMA_BOY:
    case VICTRON:
    case FRONIUS_MODBUS:
    case HUAWEI_MODBUS:
    case SMA_ISLAND:
    case SCHNEIDER:
    case WIBEEE_MODBUS:
    case INGETEAM:
    case SOLAREDGE:
      return new ModbusTcpManager();

    default:
      return new HttpInverterManager(); // SOLAX_V2 é o modo por omissão de fábrica
  }
}
