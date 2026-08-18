/*
  ModbusTcpManager.h - Gestor de excedentes para fontes Modbus TCP

  Substitui modbustcp.ino: usa a biblioteca já vendorizada em
  lib/esp32ModbusTCP (assíncrona, sobre AsyncTCP) para interrogar
  periodicamente o inversor/contador/gateway configurado em
  SurplusManagerConfig::sourceIp, com o mesmo mapa de registos/escala e a
  mesma porta por modo que existiam no ficheiro original:

  - SMA_BOY, SMA_ISLAND, VICTRON, HUAWEI_MODBUS, SCHNEIDER, WIBEEE_MODBUS:
    porta 502, todos os registos do modo pedidos em cada ciclo de
    pollIntervalMs.
  - FRONIUS_MODBUS: porta 502, mas em vez de pedir todos os registos de
    cada vez, avança um registo por ciclo (round-robin), tal como o
    fronius() original, para não sobrecarregar o Fronius Datamanager.
  - INGETEAM: porta 502, mas usando "Read Input Registers" (função 0x04)
    em vez de "Read Holding Registers", tal como o ingeteamModbus()
    original.
  - SOLAREDGE: porta 1502 (em vez de 502) - único modo com porta
    diferente, exactamente como no original (checkModbusConnection(1502)).

  A biblioteca esp32ModbusTCP já trata da ligação/reconexão ao socket de
  forma assíncrona: um novo pedido enquanto desligado despoleta connect()
  automaticamente. Este gestor só precisa de voltar a enfileirar pedidos a
  cada pollIntervalMs e de recriar a instância quando o IP, a porta ou o
  modo mudam.

  Nem todos os valores lidos pelo modbustcp.ino original têm campo
  equivalente em PowerReading (p.ex. tensão/corrente da bateria em modos
  SMA_ISLAND/VICTRON/SCHNEIDER, ou o registo "gridv" duplicado em
  INGETEAM/WIBEEE_MODBUS) - ver comentários em ModbusTcpManager.cpp para o
  que foi descartado em cada modo.

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include <IPAddress.h>
#include <esp32ModbusTCP.h>

#include <vector>

#include "SurplusManager.h"

class ModbusTcpManager : public SurplusManagerBase {
public:
  ModbusTcpManager();
  ~ModbusTcpManager() override;

  void begin() override;
  void loop() override;
  const char *name() const override;

private:
  // Tipos de valor exactamente como valueType em modbustcp.ino original.
  enum class ValueType : uint8_t {
    U16FIX0,
    U16FIX1,
    U16FIX2,
    U16FIX3,
    U16FIX10,
    U32FIX0,
    U32FIX1,
    U32FIX2,
    U32FIX3,
    S16FIX0,
    S16FIX1,
    S16FIX2,
    S16FIX3,
    S16FIX10,
    S32FIX0,
    S32FIX1,
    S32FIX2,
    S32FIX3,
    F32FIX0,
    FroniusSolarToday,
    FroniusScale,
    FroniusPv1,
    FroniusPv2,
    SunnyBoyGrid,
    SolarEdgeInverter,
    SolarEdgeMeter,
    Wibeee,
    Schneider1,
    Schneider2,
    Ingeteam,
  };

  // Equivalente a "registerData" em modbustcp.ino original, mas "variable"
  // aponta para dentro de working_ (membro desta instância) em vez de um
  // global; "functionCode" torna explícito qual função Modbus usar (só o
  // INGETEAM usa Read Input Registers - os outros usam Read Holding
  // Registers, como no original).
  struct RegisterEntry {
    float *variable;
    uint8_t serverId;
    uint16_t address;
    uint16_t length;
    ValueType type;
    esp32Modbus::FunctionCode functionCode;
  };

  void ensureConnection();
  void bindCallbacks();
  void configureRegisters();
  void sendAllRegisters();
  void tickFronius(uint32_t now, uint32_t pollIntervalMs);
  void applyRegister(const RegisterEntry &entry, uint8_t *data);
  void handleModeSpecificSignFlip(const RegisterEntry &entry);

  // Parsers "compostos" (lêem vários campos de um único bloco de
  // registos) - cópia directa das funções parseXxx() do original.
  void parseFroniusSolarToday(const uint8_t *data);
  void parseFroniusScale(const uint8_t *data);
  void parseFroniusPv1(const uint8_t *data);
  void parseFroniusPv2(const uint8_t *data);
  void parseSunnyBoyGrid(const uint8_t *data);
  void parseSolarEdgeInverter(const uint8_t *data);
  void parseSolarEdgeMeter(const uint8_t *data);
  void parseWibeeeModbus(const uint8_t *data);
  void parseSchneiderModbus1(const uint8_t *data);
  void parseSchneiderModbus2(const uint8_t *data);
  void parseIngeteamModbus(const uint8_t *data);

  static float parseFloat32(const uint8_t *data, int precision);
  static float parseUnsigned16(const uint8_t *data, int precision);
  static float parseUnsigned32(const uint8_t *data, int precision);
  static float parseSigned16(const uint8_t *data, int precision);
  static float parseSigned32(const uint8_t *data, int precision);

  esp32ModbusTCP *modbus_ = nullptr;
  IPAddress serverIp_;
  uint16_t port_ = 502;
  uint8_t mode_ = 0;
  std::vector<RegisterEntry> registers_;
  uint32_t lastRequestMs_ = 0;

  // Leitura acumulada entre pedidos (várias respostas assíncronas vão
  // preenchendo campos diferentes do mesmo working_, tal como os globais
  // "inverter"/"meter" no original).
  PowerReading working_;

  // Valores lidos mas sem campo equivalente em PowerReading (tensão/
  // corrente de bateria em modos que reutilizavam meter.voltage/current
  // para esse fim; corrente/potência AC-in/AC-out do Victron - ver
  // comentários no .cpp).
  float batteryVoltageRaw_ = 0;
  float batteryCurrentRaw_ = 0;
  float acInRaw_ = 0;
  float acOutRaw_ = 0;

  // Estado do round-robin do Fronius (froniusVariables no original).
  bool froniusPending_ = false;
  bool froniusDataReceived_ = false;
  uint32_t froniusSentMs_ = 0;
  uint8_t froniusRegisterIndex_ = 0;
  uint16_t froniusScaleA_ = 1; // corrente
  uint16_t froniusScaleV_ = 1; // tensão
  uint16_t froniusScaleW_ = 1; // potência
};
