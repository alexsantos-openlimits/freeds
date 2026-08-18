/*
  ModbusTcpManager.cpp - ver ModbusTcpManager.h

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#include "managers/ModbusTcpManager.h"

#include <math.h>

#include "AppConfig.h"
#include "Logger.h"
#include "workingmode.h"

using FC = esp32Modbus::FunctionCode;

ModbusTcpManager::ModbusTcpManager() {}

ModbusTcpManager::~ModbusTcpManager() {
  delete modbus_;
}

void ModbusTcpManager::begin() {
  ensureConnection();
}

void ModbusTcpManager::loop() {
  ensureConnection();
  if (!modbus_ || registers_.empty()) { return; }

  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
  uint32_t interval = cfg.pollIntervalMs > 0 ? cfg.pollIntervalMs : 1000;
  uint32_t now = millis();

  if (mode_ == FRONIUS_MODBUS) {
    tickFronius(now, interval);
    return;
  }

  if (now - lastRequestMs_ < interval) { return; }
  lastRequestMs_ = now;
  sendAllRegisters();
}

const char *ModbusTcpManager::name() const {
  switch (ConfigStore::get().surplus.mode) {
    case SMA_BOY:
      return "Modbus TCP SMA Sunny Boy";
    case VICTRON:
      return "Modbus TCP Victron";
    case FRONIUS_MODBUS:
      return "Modbus TCP Fronius";
    case HUAWEI_MODBUS:
      return "Modbus TCP Huawei";
    case SMA_ISLAND:
      return "Modbus TCP SMA Sunny Island";
    case SCHNEIDER:
      return "Modbus TCP Schneider";
    case WIBEEE_MODBUS:
      return "Modbus TCP Wibeee";
    case INGETEAM:
      return "Modbus TCP Ingeteam";
    case SOLAREDGE:
      return "Modbus TCP SolarEdge";
    default:
      return "Modbus TCP";
  }
}

// ---------------------------------------------------------------------------
// Ligação / reconexão
// ---------------------------------------------------------------------------

void ModbusTcpManager::ensureConnection() {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  IPAddress ip;
  ip.fromString(cfg.sourceIp);
  // Único modo com porta diferente de 502 - idêntico a checkModbusConnection()
  // do modbustcp.ino original (solarEdge() chama-o com 1502).
  uint16_t port = (cfg.mode == SOLAREDGE) ? 1502 : 502;

  bool needsRebuild = (modbus_ == nullptr) || (cfg.mode != mode_) || (ip != serverIp_) || (port != port_);
  if (!needsRebuild) { return; }

  if (modbus_) {
    delete modbus_;
    modbus_ = nullptr;
  }

  mode_ = cfg.mode;
  serverIp_ = ip;
  port_ = port;
  working_ = PowerReading();
  lastRequestMs_ = 0;

  modbus_ = new esp32ModbusTCP(serverIp_, port_);
  bindCallbacks();
  configureRegisters();

  Logger::info("ModbusTcpManager: a ligar a %s:%u (modo %u, %u registo(s))\n",
               cfg.sourceIp, (unsigned)port_, (unsigned)mode_, (unsigned)registers_.size());
}

void ModbusTcpManager::bindCallbacks() {
  modbus_->onData([this](uint16_t packetId, uint8_t slave, esp32Modbus::FunctionCode fc, uint8_t *data, uint16_t len, void *arg) {
    RegisterEntry *entry = reinterpret_cast<RegisterEntry *>(arg);
    if (!entry) { return; }

    applyRegister(*entry, data);
    handleModeSpecificSignFlip(*entry);

    publishReading(working_);
  });

  modbus_->onError([this](uint16_t packetId, esp32Modbus::Error error, void *arg) {
    RegisterEntry *entry = reinterpret_cast<RegisterEntry *>(arg);
    Logger::info("ModbusTcpManager: erro no pacote %u endereco %u: 0x%02X\n",
                 (unsigned)packetId, entry ? (unsigned)entry->address : 0u, (unsigned)error);
    if (mode_ == FRONIUS_MODBUS) { froniusPending_ = false; }
  });
}

// ---------------------------------------------------------------------------
// Mapas de registos por modo - cópia directa de smaRegistersBoy[],
// victronRegisters[], froniusRegisters[], etc. do modbustcp.ino original.
//
// Nem todos os valores lidos ali têm campo equivalente em PowerReading:
// - SMA_ISLAND (30843/30851) e VICTRON (840/841): tensão/corrente de
//   BATERIA (não de rede, apesar do original reutilizar meter.voltage/
//   current para os guardar) - sem campo batteryVoltage/batteryCurrent em
//   PowerReading, ficam só em batteryVoltageRaw_/batteryCurrentRaw_
//   (não publicados).
// - VICTRON (811/808, acIn/acOut): sem campo equivalente - acInRaw_/
//   acOutRaw_, não publicados.
// - INGETEAM (registo 49) e WIBEEE_MODBUS (registo 59): segunda leitura
//   de tensão de rede que o original guardava em inverter.gridv,
//   distinta da guardada em meter.voltage; como PowerReading só tem um
//   gridVoltage, mantém-se o valor do contador de rede (meter.voltage no
//   original) e descarta-se o duplicado - ver parseIngeteamModbus()/
//   parseWibeeeModbus().
// ---------------------------------------------------------------------------

void ModbusTcpManager::configureRegisters() {
  registers_.clear();
  fields_ = PowerReadingFields();
  batteryVoltageRaw_ = 0;
  batteryCurrentRaw_ = 0;
  acInRaw_ = 0;
  acOutRaw_ = 0;
  froniusRegisterIndex_ = 0;
  froniusPending_ = false;
  froniusDataReceived_ = false;
  froniusScaleA_ = froniusScaleV_ = froniusScaleW_ = 1;

  switch (mode_) {
    case SMA_BOY:
      registers_ = {
          {&working_.solarWattsToday, 3, 30535, 2, ValueType::U32FIX3, FC::READ_HOLD_REGISTER},
          {&working_.pv1Current, 3, 30769, 2, ValueType::S32FIX3, FC::READ_HOLD_REGISTER},
          {&working_.pv1Voltage, 3, 30771, 2, ValueType::S32FIX2, FC::READ_HOLD_REGISTER},
          {&working_.pv1Watts, 3, 30773, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.solarWatts, 3, 30775, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.pv2Current, 3, 30957, 2, ValueType::S32FIX3, FC::READ_HOLD_REGISTER},
          {&working_.pv2Voltage, 3, 30959, 2, ValueType::S32FIX2, FC::READ_HOLD_REGISTER},
          {&working_.pv2Watts, 3, 30961, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.gridWatts, 3, 30865, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.gridWatts, 3, 30867, 2, ValueType::SunnyBoyGrid, FC::READ_HOLD_REGISTER},
      };
      fields_.solarWattsToday = true;
      fields_.pv1 = true;
      fields_.pv2 = true;
      fields_.solarWatts = true;
      fields_.gridWatts = true;
      break;

    case SMA_ISLAND:
      registers_ = {
          {&working_.batterySoc, 3, 30845, 2, ValueType::U32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.batteryWatts, 3, 30775, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.inverterTemperature, 3, 30849, 2, ValueType::S32FIX1, FC::READ_HOLD_REGISTER},
          {&batteryCurrentRaw_, 3, 30843, 2, ValueType::S32FIX3, FC::READ_HOLD_REGISTER},
          {&batteryVoltageRaw_, 3, 30851, 2, ValueType::U32FIX2, FC::READ_HOLD_REGISTER},
      };
      fields_.batterySoc = true;
      fields_.batteryWatts = true;
      fields_.inverterTemperature = true;
      break;

    case VICTRON:
      registers_ = {
          {&batteryVoltageRaw_, 100, 840, 1, ValueType::U16FIX1, FC::READ_HOLD_REGISTER},
          {&batteryCurrentRaw_, 100, 841, 1, ValueType::S16FIX1, FC::READ_HOLD_REGISTER},
          {&working_.batteryWatts, 100, 842, 1, ValueType::S16FIX0, FC::READ_HOLD_REGISTER},
          {&working_.batterySoc, 100, 843, 1, ValueType::U16FIX0, FC::READ_HOLD_REGISTER},
          {&acInRaw_, 100, 811, 1, ValueType::U16FIX0, FC::READ_HOLD_REGISTER},
          {&acOutRaw_, 100, 808, 1, ValueType::U16FIX0, FC::READ_HOLD_REGISTER},
          {&working_.solarWatts, 100, 850, 1, ValueType::U16FIX0, FC::READ_HOLD_REGISTER},
          {&working_.gridWatts, 100, 820, 1, ValueType::S16FIX0, FC::READ_HOLD_REGISTER},
          {&working_.loadWatts, 100, 817, 1, ValueType::U16FIX0, FC::READ_HOLD_REGISTER},
      };
      fields_.batteryWatts = true;
      fields_.batterySoc = true;
      fields_.solarWatts = true;
      fields_.gridWatts = true;
      fields_.loadWatts = true;
      break;

    case FRONIUS_MODBUS:
      // Round-robin: um pedido por ciclo (ver tickFronius()), não todos de
      // uma vez, tal como fronius() original.
      registers_ = {
          {nullptr, 1, 499, 6, ValueType::FroniusSolarToday, FC::READ_HOLD_REGISTER},
          {nullptr, 1, 40265, 3, ValueType::FroniusScale, FC::READ_HOLD_REGISTER},
          {nullptr, 1, 40282, 3, ValueType::FroniusPv1, FC::READ_HOLD_REGISTER},
          {nullptr, 1, 40302, 3, ValueType::FroniusPv2, FC::READ_HOLD_REGISTER},
          {&working_.gridWatts, 240, 40097, 2, ValueType::F32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.gridVoltage, 240, 40079, 2, ValueType::F32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.gridCurrent, 240, 40071, 2, ValueType::F32FIX0, FC::READ_HOLD_REGISTER},
      };
      fields_.solarWatts = true;
      fields_.solarWattsToday = true;
      fields_.pv1 = true;
      fields_.pv2 = true;
      fields_.gridWatts = true;
      fields_.gridVoltage = true;
      fields_.gridCurrent = true;
      break;

    case HUAWEI_MODBUS:
      registers_ = {
          {&working_.gridWatts, 0, 37113, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.solarWattsToday, 0, 32114, 2, ValueType::U32FIX2, FC::READ_HOLD_REGISTER},
          {&working_.solarWatts, 0, 32064, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
          {&working_.pv1Voltage, 0, 32016, 1, ValueType::S16FIX1, FC::READ_HOLD_REGISTER},
          {&working_.pv1Current, 0, 32017, 1, ValueType::S16FIX2, FC::READ_HOLD_REGISTER},
          {&working_.pv2Voltage, 0, 32018, 1, ValueType::S16FIX1, FC::READ_HOLD_REGISTER},
          {&working_.pv2Current, 0, 32019, 1, ValueType::S16FIX2, FC::READ_HOLD_REGISTER},
          {&working_.inverterTemperature, 0, 32087, 1, ValueType::S16FIX1, FC::READ_HOLD_REGISTER},
          {&working_.batteryWatts, 0, 37001, 2, ValueType::S32FIX0, FC::READ_HOLD_REGISTER},
      };
      fields_.gridWatts = true;
      fields_.solarWattsToday = true;
      fields_.solarWatts = true;
      fields_.pv1 = true;
      fields_.pv2 = true;
      fields_.inverterTemperature = true;
      fields_.batteryWatts = true;
      break;

    case SOLAREDGE:
      registers_ = {
          {&working_.solarWatts, 1, 40083, 23, ValueType::SolarEdgeInverter, FC::READ_HOLD_REGISTER},
          {&working_.gridWatts, 1, 40206, 4, ValueType::SolarEdgeMeter, FC::READ_HOLD_REGISTER},
      };
      fields_.solarWatts = true;
      fields_.pv1 = true;
      fields_.inverterTemperature = true;
      fields_.gridWatts = true;
      break;

    case WIBEEE_MODBUS:
      registers_ = {
          {&working_.gridWatts, 1, 0, 72, ValueType::Wibeee, FC::READ_HOLD_REGISTER},
      };
      fields_.gridWatts = true;
      fields_.solarWatts = true;
      fields_.gridCurrent = true;
      fields_.gridVoltage = true;
      fields_.frequency = true;
      fields_.powerFactor = true;
      fields_.importActiveEnergy = true;
      break;

    case INGETEAM:
      // Único modo que usa "Read Input Registers" (0x04) em vez de "Read
      // Holding Registers" (0x03), tal como ingeteamModbus() original.
      registers_ = {
          {&working_.gridWatts, 1, 0, 73, ValueType::Ingeteam, FC::READ_INPUT_REGISTER},
      };
      fields_.gridVoltage = true;
      fields_.gridCurrent = true;
      fields_.batteryWatts = true;
      fields_.batterySoc = true;
      fields_.pv1 = true;
      fields_.pv2 = true;
      fields_.solarWatts = true;
      fields_.loadWatts = true;
      fields_.frequency = true;
      fields_.gridWatts = true;
      fields_.inverterTemperature = true;
      break;

    case SCHNEIDER:
      registers_ = {
          {&working_.loadWatts, 201, 98, 66, ValueType::Schneider1, FC::READ_HOLD_REGISTER},
          {&working_.solarWatts, 201, 354, 4, ValueType::Schneider2, FC::READ_HOLD_REGISTER},
      };
      fields_.loadWatts = true;
      fields_.inverterTemperature = true;
      fields_.batteryWatts = true;
      fields_.solarWatts = true;
      break;

    default:
      Logger::info("ModbusTcpManager: modo %u nao suportado\n", (unsigned)mode_);
      break;
  }
}

void ModbusTcpManager::sendAllRegisters() {
  for (auto &entry : registers_) {
    uint16_t id;
    if (entry.functionCode == FC::READ_INPUT_REGISTER) {
      id = modbus_->readInputRegisters(entry.serverId, entry.address, entry.length, (void *)&entry);
    } else {
      id = modbus_->readHoldingRegisters(entry.serverId, entry.address, entry.length, (void *)&entry);
    }
    if (id == 0) {
      Logger::info("ModbusTcpManager: erro ao pedir registo %u (modo %u)\n", (unsigned)entry.address, (unsigned)mode_);
    }
  }

  if (mode_ == HUAWEI_MODBUS) {
    // O original calcula pw1/pw2 logo após emitir os pedidos, usando os
    // valores de tensão/corrente ainda da leitura anterior (as respostas
    // a estes pedidos só chegam de forma assíncrona, mais tarde) -
    // replicado tal e qual, incluindo essa "desactualização" de um ciclo.
    working_.pv1Watts = working_.pv1Voltage * working_.pv1Current;
    working_.pv2Watts = working_.pv2Voltage * working_.pv2Current;
    publishReading(working_);
  }
}

void ModbusTcpManager::tickFronius(uint32_t now, uint32_t pollIntervalMs) {
  if (registers_.empty()) { return; }
  if (froniusRegisterIndex_ >= registers_.size()) { froniusRegisterIndex_ = 0; }
  if (froniusPending_ && (now - froniusSentMs_ > 10000)) { froniusPending_ = false; }

  // Só avança quando chegou uma resposta (froniusDataReceived_) ou quando
  // não há nenhum pedido pendente (ou este expirou) - tal como
  // "data_ready || !froniusRequestSend" no fronius() original.
  if (froniusPending_ && !froniusDataReceived_) { return; }
  if (now - lastRequestMs_ < pollIntervalMs) { return; }

  lastRequestMs_ = now;
  froniusDataReceived_ = false;
  froniusPending_ = false;
  froniusSentMs_ = now;

  RegisterEntry &entry = registers_[froniusRegisterIndex_];
  uint16_t id = modbus_->readHoldingRegisters(entry.serverId, entry.address, entry.length, (void *)&entry);
  if (id > 0) {
    froniusPending_ = true;
  } else {
    Logger::info("ModbusTcpManager (Fronius): erro ao pedir registo %u\n", (unsigned)entry.address);
  }
}

// ---------------------------------------------------------------------------
// Sinal da rede / regras específicas por modo - cópia directa das
// condições finais de configModbusTcp() do original.
// ---------------------------------------------------------------------------

void ModbusTcpManager::handleModeSpecificSignFlip(const RegisterEntry &entry) {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  if (mode_ == FRONIUS_MODBUS) {
    if (entry.address == 40097 && !cfg.changeGridSign) { working_.gridWatts *= -1.0f; }
    froniusDataReceived_ = true;
    froniusRegisterIndex_++;
  }

  if (mode_ == SMA_BOY && entry.address == 30867 && !cfg.changeGridSign) { working_.gridWatts *= -1.0f; }
  if (mode_ == SMA_ISLAND && entry.address == 30775 && !cfg.changeGridSign) { working_.batteryWatts *= -1.0f; }
  if (mode_ == VICTRON && entry.address == 820 && !cfg.changeGridSign) { working_.gridWatts *= -1.0f; }
  if (mode_ == VICTRON && entry.address == 850) {
    // Função customizada para o Aeizoon (comentário do original): se não
    // houver BMV dedicado, soma-se a produção solar à potência de bateria.
    if (!cfg.useBmv) { working_.batteryWatts += working_.solarWatts; }
  }
  if (mode_ == HUAWEI_MODBUS && entry.address == 37113 && cfg.changeGridSign) { working_.gridWatts *= -1.0f; }
  if (mode_ == SOLAREDGE && entry.address == 40206 && cfg.changeGridSign) { working_.gridWatts *= -1.0f; }
}

// ---------------------------------------------------------------------------
// Despacho por tipo de valor - cópia directa do onData() de
// configModbusTcp() original.
// ---------------------------------------------------------------------------

void ModbusTcpManager::applyRegister(const RegisterEntry &entry, uint8_t *data) {
  switch (entry.type) {
    case ValueType::U16FIX0: *entry.variable = parseUnsigned16(data, 0); break;
    case ValueType::U16FIX1: *entry.variable = parseUnsigned16(data, 1); break;
    case ValueType::U16FIX2: *entry.variable = parseUnsigned16(data, 2); break;
    case ValueType::U16FIX3: *entry.variable = parseUnsigned16(data, 3); break;
    case ValueType::U16FIX10: *entry.variable = parseUnsigned16(data, 10); break;
    case ValueType::U32FIX0: *entry.variable = parseUnsigned32(data, 0); break;
    case ValueType::U32FIX1: *entry.variable = parseUnsigned32(data, 1); break;
    case ValueType::U32FIX2: *entry.variable = parseUnsigned32(data, 2); break;
    case ValueType::U32FIX3: *entry.variable = parseUnsigned32(data, 3); break;
    case ValueType::S16FIX0: *entry.variable = parseSigned16(data, 0); break;
    case ValueType::S16FIX1: *entry.variable = parseSigned16(data, 1); break;
    case ValueType::S16FIX2: *entry.variable = parseSigned16(data, 2); break;
    case ValueType::S16FIX3: *entry.variable = parseSigned16(data, 3); break;
    case ValueType::S16FIX10: *entry.variable = parseSigned16(data, 10); break;
    case ValueType::S32FIX0: *entry.variable = parseSigned32(data, 0); break;
    case ValueType::S32FIX1: *entry.variable = parseSigned32(data, 1); break;
    case ValueType::S32FIX2: *entry.variable = parseSigned32(data, 2); break;
    case ValueType::S32FIX3: *entry.variable = parseSigned32(data, 3); break;
    case ValueType::F32FIX0: *entry.variable = parseFloat32(data, 0); break;
    case ValueType::FroniusSolarToday: parseFroniusSolarToday(data); break;
    case ValueType::FroniusScale: parseFroniusScale(data); break;
    case ValueType::FroniusPv1: parseFroniusPv1(data); break;
    case ValueType::FroniusPv2: parseFroniusPv2(data); break;
    case ValueType::SunnyBoyGrid: parseSunnyBoyGrid(data); break;
    case ValueType::SolarEdgeInverter: parseSolarEdgeInverter(data); break;
    case ValueType::SolarEdgeMeter: parseSolarEdgeMeter(data); break;
    case ValueType::Wibeee: parseWibeeeModbus(data); break;
    case ValueType::Schneider1: parseSchneiderModbus1(data); break;
    case ValueType::Schneider2: parseSchneiderModbus2(data); break;
    case ValueType::Ingeteam: parseIngeteamModbus(data); break;
  }
}

// ---------------------------------------------------------------------------
// Descodificadores de valor simples - cópia directa de parseFloat32()/
// parseUnsignedNN()/parseSignedNN() do original.
// ---------------------------------------------------------------------------

float ModbusTcpManager::parseFloat32(const uint8_t *data, int precision) {
  float value = 0;
  *((unsigned char *)&value + 3) = data[0];
  *((unsigned char *)&value + 2) = data[1];
  *((unsigned char *)&value + 1) = data[2];
  *((unsigned char *)&value + 0) = data[3];

  switch (precision) {
    case 0: return value;
    case 1: return value / 10.0f;
    case 2: return value / 100.0f;
    case 3: return value / 1000.0f;
  }
  return 0;
}

float ModbusTcpManager::parseUnsigned16(const uint8_t *data, int precision) {
  uint16_t value = (uint16_t)((data[0] << 8) | data[1]);

  switch (precision) {
    case 0: return (float)value;
    case 1: return (float)value / 10.0f;
    case 2: return (float)value / 100.0f;
    case 3: return (float)value / 1000.0f;
    case 10: return (float)value * 10.0f;
  }
  return 0;
}

float ModbusTcpManager::parseUnsigned32(const uint8_t *data, int precision) {
  uint32_t value = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];

  switch (precision) {
    case 0: return (float)value;
    case 1: return (float)value / 10.0f;
    case 2: return (float)value / 100.0f;
    case 3: return (float)value / 1000.0f;
  }
  return 0;
}

float ModbusTcpManager::parseSigned16(const uint8_t *data, int precision) {
  int16_t value = (int16_t)((data[0] << 8) | data[1]);

  switch (precision) {
    case 0: return (float)value;
    case 1: return (float)value / 10.0f;
    case 2: return (float)value / 100.0f;
    case 3: return (float)value / 1000.0f;
    case 10: return (float)value * 10.0f;
  }
  return 0;
}

float ModbusTcpManager::parseSigned32(const uint8_t *data, int precision) {
  int32_t value = (int32_t)(((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3]);

  if ((uint32_t)value == 0x80000000u) { return 0; } // sanitizador tal como o original

  switch (precision) {
    case 0: return (float)value;
    case 1: return (float)value / 10.0f;
    case 2: return (float)value / 100.0f;
    case 3: return (float)value / 1000.0f;
  }
  return 0;
}

// ---------------------------------------------------------------------------
// Descodificadores compostos (vários campos por bloco de registos) -
// cópia directa das funções parseXxx() do original, com os globais
// inverter.*/meter.* substituídos por working_.* (ou pelos escalares de
// apoio froniusScale*_/batteryVoltageRaw_/batteryCurrentRaw_).
// ---------------------------------------------------------------------------

void ModbusTcpManager::parseFroniusSolarToday(const uint8_t *data) {
  working_.solarWatts = (float)(((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3]);

  uint32_t high = ((uint32_t)data[4] << 24) | ((uint32_t)data[5] << 16) | ((uint32_t)data[6] << 8) | data[7];
  uint32_t low = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 8) | data[11];
  uint64_t value = (((uint64_t)high) << 32) | (uint64_t)low;
  working_.solarWattsToday = (float)value / 1000.0f;
}

void ModbusTcpManager::parseFroniusScale(const uint8_t *data) {
  uint16_t *targets[3] = {&froniusScaleA_, &froniusScaleV_, &froniusScaleW_};

  for (int i = 0; i < 3; i++) {
    int16_t value = (int16_t)((data[i * 2] << 8) | data[(i * 2) + 1]);
    switch (value) {
      case 0: *targets[i] = 1; break;
      case -1: *targets[i] = 10; break;
      case -2: *targets[i] = 100; break;
      case -3: *targets[i] = 1000; break;
      default: break;
    }
  }
}

void ModbusTcpManager::parseFroniusPv1(const uint8_t *data) {
  uint16_t value = (uint16_t)((data[0] << 8) | data[1]);
  if (value != 0xFFFF) { working_.pv1Current = (float)value / froniusScaleA_; }
  value = (uint16_t)((data[2] << 8) | data[3]);
  if (value != 0xFFFF) { working_.pv1Voltage = (float)value / froniusScaleV_; }
  value = (uint16_t)((data[4] << 8) | data[5]);
  if (value != 0xFFFF) { working_.pv1Watts = (float)value / froniusScaleW_; }
}

void ModbusTcpManager::parseFroniusPv2(const uint8_t *data) {
  uint16_t value = (uint16_t)((data[0] << 8) | data[1]);
  if (value != 0xFFFF) { working_.pv2Current = (float)value / froniusScaleA_; }
  value = (uint16_t)((data[2] << 8) | data[3]);
  if (value != 0xFFFF) { working_.pv2Voltage = (float)value / froniusScaleV_; }
  value = (uint16_t)((data[4] << 8) | data[5]);
  if (value != 0xFFFF) { working_.pv2Watts = (float)value / froniusScaleW_; }
}

void ModbusTcpManager::parseSunnyBoyGrid(const uint8_t *data) {
  int32_t exported = (int32_t)(((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3]);
  working_.gridWatts -= (float)exported;
}

void ModbusTcpManager::parseSolarEdgeInverter(const uint8_t *data) {
  int16_t value;
  int16_t sf;

  // Registos 40084-40085: potência solar instantânea + escala.
  value = (int16_t)((data[0] << 8) | data[1]);
  sf = (int16_t)((data[2] << 8) | data[3]);
  working_.solarWatts = value * powf(10.0f, sf);

  // Registos 40086-40096 ignorados (tal como no original).

  // Registos 40097-40102: corrente/tensão/potência do MPPT1 + escalas.
  value = (int16_t)((data[26] << 8) | data[27]);
  sf = (int16_t)((data[28] << 8) | data[29]);
  working_.pv1Current = value * powf(10.0f, sf);

  value = (int16_t)((data[30] << 8) | data[31]);
  sf = (int16_t)((data[32] << 8) | data[33]);
  working_.pv1Voltage = value * powf(10.0f, sf);

  value = (int16_t)((data[34] << 8) | data[35]);
  sf = (int16_t)((data[36] << 8) | data[37]);
  working_.pv1Watts = value * powf(10.0f, sf);

  // Registo 40103 ignorado.

  // Registos 40104-40107: temperatura + escala.
  value = (int16_t)((data[40] << 8) | data[41]);
  sf = (int16_t)((data[46] << 8) | data[47]);
  working_.inverterTemperature = value * powf(10.0f, sf);
}

void ModbusTcpManager::parseSolarEdgeMeter(const uint8_t *data) {
  int16_t value = (int16_t)((data[0] << 8) | data[1]);
  int16_t sf = (int16_t)((data[6] << 8) | data[7]);
  working_.gridWatts = value * powf(10.0f, sf);
}

void ModbusTcpManager::parseIngeteamModbus(const uint8_t *data) {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  int16_t value;
  uint16_t uvalue;

  // Registo 18 - tensão do contador de rede.
  uvalue = (uint16_t)((data[34] << 8) | data[35]);
  working_.gridVoltage = (float)uvalue / 10.0f;

  // Registo 19 - corrente do contador de rede.
  value = (int16_t)((data[36] << 8) | data[37]);
  working_.gridCurrent = (float)value / 100.0f;

  // Registo 20 - potência de bateria.
  value = (int16_t)((data[38] << 8) | data[39]);
  working_.batteryWatts = (float)value * -1.0f;

  // Registo 21 - SoC de bateria.
  uvalue = (uint16_t)((data[40] << 8) | data[41]);
  working_.batterySoc = (float)uvalue;

  // Registos 32/33/34 - tensão/corrente/potência do MPPT1.
  uvalue = (uint16_t)((data[62] << 8) | data[63]);
  working_.pv1Voltage = (float)uvalue;
  uvalue = (uint16_t)((data[64] << 8) | data[65]);
  working_.pv1Current = (float)uvalue / 100.0f;
  uvalue = (uint16_t)((data[66] << 8) | data[67]);
  working_.pv1Watts = (float)uvalue;

  // Registos 35/36/37 - tensão/corrente/potência do MPPT2.
  uvalue = (uint16_t)((data[68] << 8) | data[69]);
  working_.pv2Voltage = (float)uvalue;
  uvalue = (uint16_t)((data[70] << 8) | data[71]);
  working_.pv2Current = (float)uvalue / 100.0f;
  uvalue = (uint16_t)((data[72] << 8) | data[73]);
  working_.pv2Watts = (float)uvalue;

  working_.solarWatts = working_.pv1Watts + working_.pv2Watts;

  // Registo 38 - potência de consumo local.
  value = (int16_t)((data[74] << 8) | data[75]);
  working_.loadWatts = (float)value;

  // Registo 49 - segunda leitura de tensão de rede (medida pelo próprio
  // inversor); PowerReading só tem um gridVoltage e este já foi
  // preenchido a partir do registo 18 (contador externo) - descartado.

  // Registo 51 - frequência.
  uvalue = (uint16_t)((data[100] << 8) | data[101]);
  working_.frequency = (float)uvalue / 100.0f;

  // Registo 52 (ou 72, se useExternalMeter) - potência de rede.
  value = cfg.useExternalMeter ? (int16_t)((data[142] << 8) | data[143]) : (int16_t)((data[102] << 8) | data[103]);
  working_.gridWatts = (float)value;
  if (cfg.changeGridSign) { working_.gridWatts *= -1.0f; }

  // Registo 58 - temperatura do inversor.
  value = (int16_t)((data[114] << 8) | data[115]);
  working_.inverterTemperature = value / 10.0f;
}

void ModbusTcpManager::parseWibeeeModbus(const uint8_t *data) {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  uint16_t uvalue;
  int16_t value;

  // Registo 0 - potência de rede.
  uvalue = (uint16_t)((data[0] << 8) | data[1]);
  working_.gridWatts = (float)uvalue;

  // Registo 2 - potência solar.
  uvalue = (uint16_t)((data[4] << 8) | data[5]);
  working_.solarWatts = (float)uvalue;

  // Registo 24 - energia activa importada.
  uvalue = (uint16_t)((data[48] << 8) | data[49]);
  working_.importActiveEnergy = (float)uvalue / 100.0f;

  // Registo 48 - corrente.
  uvalue = (uint16_t)((data[96] << 8) | data[97]);
  working_.gridCurrent = (float)uvalue / 100.0f;

  // Registo 58 - tensão.
  uvalue = (uint16_t)((data[116] << 8) | data[117]);
  working_.gridVoltage = (float)uvalue / 100.0f;

  // Registo 59 - segunda leitura de tensão (guardada em inverter.gridv no
  // original); sem segundo campo de tensão em PowerReading - descartado.

  // Registo 62 - frequência.
  uvalue = (uint16_t)((data[124] << 8) | data[125]);
  working_.frequency = (float)uvalue / 100.0f;

  // Registo 66 - factor de potência; o sinal indica o sentido do fluxo de
  // energia e é usado, tal como no original, para decidir se inverte
  // gridWatts.
  value = (int16_t)((data[132] << 8) | data[133]);
  working_.powerFactor = (float)value / 100.0f;

  if (working_.powerFactor > 0) {
    if (!cfg.changeGridSign) { working_.gridWatts *= -1.0f; }
  } else {
    working_.powerFactor *= -1.0f;
    if (cfg.changeGridSign) { working_.gridWatts *= -1.0f; }
  }
}

void ModbusTcpManager::parseSchneiderModbus1(const uint8_t *data) {
  // Combox Schneider: little endian, palavras trocadas (ver comentário
  // "LITTLE ENDIAN COMBOX" no original).

  // Registo 98 (0x62) - potência de consumo local.
  uint32_t uvalue = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[0] << 8) | data[1];
  working_.loadWatts = (float)uvalue;

  // Registo 152 (0x98) - tensão de bateria (sem campo próprio em
  // PowerReading; guardada apenas para calcular batteryWatts a seguir).
  uvalue = ((uint32_t)data[110] << 24) | ((uint32_t)data[111] << 16) | ((uint32_t)data[108] << 8) | data[109];
  batteryVoltageRaw_ = uvalue * 0.001f;

  // Registo 154 (0x9A) - temperatura.
  uvalue = ((uint32_t)data[114] << 24) | ((uint32_t)data[115] << 16) | ((uint32_t)data[112] << 8) | data[113];
  working_.inverterTemperature = (uvalue * 0.01f) - 273.0f;

  // Registo 156 (0x9B) - corrente de bateria (idem, só para o cálculo).
  int32_t value = (int32_t)(((uint32_t)data[118] << 24) | ((uint32_t)data[119] << 16) | ((uint32_t)data[116] << 8) | data[117]);
  batteryCurrentRaw_ = value * 0.001f;

  working_.batteryWatts = batteryVoltageRaw_ * batteryCurrentRaw_;
}

void ModbusTcpManager::parseSchneiderModbus2(const uint8_t *data) {
  // Registo 354 (0x162) - tensão do MPPT / Registo 356 (0x164) - corrente.
  uint32_t pvVoltage = ((uint32_t)data[2] << 24) | ((uint32_t)data[3] << 16) | ((uint32_t)data[0] << 8) | data[1];
  uint32_t pvCurrent = ((uint32_t)data[6] << 24) | ((uint32_t)data[7] << 16) | ((uint32_t)data[4] << 8) | data[5];

  working_.solarWatts = (pvVoltage * 0.001f) * (pvCurrent * 0.001f);
}
