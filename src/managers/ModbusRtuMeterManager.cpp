/*
  ModbusRtuMeterManager.cpp - ver ModbusRtuMeterManager.h
*/
#include "managers/ModbusRtuMeterManager.h"

#include "AppConfig.h"
#include "Logger.h"
#include "workingmode.h"

ModbusRtuMeterManager::ModbusRtuMeterManager() : serial_(1) {}

void ModbusRtuMeterManager::begin() {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  serial_.begin(cfg.meterBaud, SERIAL_8N1, kRxPin, kTxPin);

  lastMode_ = cfg.mode;
  lastBaud_ = cfg.meterBaud;
  lastTickMs_ = 0;
  sendRetry_ = 0;
  ddsu666State_ = 0;

  Logger::info("ModbusRtuMeterManager: iniciado (modo %u, %lu bps, escravo %u)\n",
               (unsigned)cfg.mode, (unsigned long)cfg.meterBaud, (unsigned)cfg.meterId);
}

void ModbusRtuMeterManager::loop() {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  if (cfg.meterBaud != lastBaud_) {
    serial_.updateBaudRate(cfg.meterBaud);
    lastBaud_ = cfg.meterBaud;
  }

  if (cfg.mode != lastMode_) {
    // Mudou o tipo de contador em tempo real: reinicia a máquina de
    // estados e descarta o que estiver pendente na série para não
    // confundir a resposta de um mapa de registos com a de outro.
    lastMode_ = cfg.mode;
    sendRetry_ = 0;
    ddsu666State_ = 0;
    while (serial_.available() > 0) { serial_.read(); }
  }

  uint32_t interval = cfg.pollIntervalMs > 0 ? cfg.pollIntervalMs : 1500;
  uint32_t now = millis();
  if (now - lastTickMs_ < interval) { return; }
  lastTickMs_ = now;

  switch (cfg.mode) {
    case DDS238_METER:
      tickDds2382();
      break;
    case DDSU666_METER:
      tickDdsu666();
      break;
    case SDM_METER:
      tickSdm120();
      break;
    case MUSTSOLAR:
      tickMustSolar();
      break;
    default:
      break;
  }
}

const char *ModbusRtuMeterManager::name() const {
  switch (ConfigStore::get().surplus.mode) {
    case DDS238_METER:
      return "Modbus DDS238-2";
    case DDSU666_METER:
      return "Modbus DDSU666";
    case SDM_METER:
      return "Modbus SDM120/220/230";
    case MUSTSOLAR:
      return "Modbus MustSolar";
    default:
      return "Modbus RTU";
  }
}

// ---------------------------------------------------------------------------
// Trama Modbus RTU (frame + CRC16) - idêntico a modbusSend()/
// modbusCalculateCRC() do modbus_functions.ino original.
// ---------------------------------------------------------------------------

uint16_t ModbusRtuMeterManager::calculateCrc(const uint8_t *frame, uint8_t len) {
  uint16_t crc = 0xFFFF;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= frame[i];
    for (uint8_t j = 8; j; j--) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }
  return crc;
}

void ModbusRtuMeterManager::sendRequest(uint8_t functionCode, uint16_t startAddress, uint16_t registerCount) {
  expectedAddress_ = ConfigStore::get().surplus.meterId;

  uint8_t frame[8];
  frame[0] = expectedAddress_;
  frame[1] = functionCode;
  frame[2] = (uint8_t)(startAddress >> 8);
  frame[3] = (uint8_t)(startAddress);
  frame[4] = (uint8_t)(registerCount >> 8);
  frame[5] = (uint8_t)(registerCount);
  uint16_t crc = calculateCrc(frame, 6);
  frame[6] = (uint8_t)(crc);
  frame[7] = (uint8_t)(crc >> 8);

  serial_.flush();
  serial_.write(frame, sizeof(frame));
}

bool ModbusRtuMeterManager::receiveReady() {
  return serial_.available() > 4;
}

uint8_t ModbusRtuMeterManager::receiveBuffer(uint8_t *buffer, uint8_t bufferSize, uint8_t registerCount) {
  uint16_t wanted = (uint16_t)registerCount * 2 + 5;
  uint8_t maxLen = wanted < bufferSize ? (uint8_t)wanted : bufferSize;

  uint8_t len = 0;
  uint32_t last = millis();
  while (serial_.available() > 0 && len < maxLen && (millis() - last < kReceiveTimeoutMs)) {
    uint8_t data = (uint8_t)serial_.read();
    if (len == 0) {
      // Ignora bytes residuais até encontrar o endereço do escravo à
      // espera, tal como o original.
      if (data == expectedAddress_) {
        buffer[len++] = data;
      }
    } else {
      buffer[len++] = data;
      if (len == 3) {
        if (buffer[1] & 0x80) {
          return buffer[2]; // código de excepção Modbus (1..11)
        }
      }
    }
    last = millis();
  }

  if (len < 7) { return 7; } // dados insuficientes

  uint16_t crc = (buffer[len - 1] << 8) | buffer[len - 2];
  if (calculateCrc(buffer, len - 2) != crc) { return 9; } // CRC inválido

  return 0;
}

float ModbusRtuMeterManager::decodeFloatBE(const uint8_t *data, uint8_t offset) {
  //  0  1  2  3
  // 43 66 33 34 = 230.2 (big endian na trama Modbus, ESP32 é little endian)
  float value;
  ((uint8_t *)&value)[3] = data[offset];
  ((uint8_t *)&value)[2] = data[offset + 1];
  ((uint8_t *)&value)[1] = data[offset + 2];
  ((uint8_t *)&value)[0] = data[offset + 3];
  return value;
}

// ---------------------------------------------------------------------------
// DDS238-2 (dds2382 original): um pedido de 18 registos, função 0x03.
// ---------------------------------------------------------------------------
void ModbusRtuMeterManager::tickDds2382() {
  bool dataReady = receiveReady();

  if (dataReady) {
    uint8_t buffer[46];
    uint8_t error = receiveBuffer(buffer, sizeof(buffer), 18);

    if (error) {
      Logger::info("ModbusRtuMeterManager (DDS238-2): erro %u\n", (unsigned)error);
    } else {
      const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
      PowerReading r = reading_;

      // energyTotal (buffer[3..6], contador acumulado do próprio
      // dispositivo) não tem campo equivalente em PowerReading; descartado.

      r.gridVoltage = (float)((buffer[27] << 8) + buffer[28]) / 10.0f;
      r.gridCurrent = (float)((buffer[29] << 8) + buffer[30]) / 100.0f;

      float activePower = (float)((int16_t)((buffer[31] << 8) + buffer[32]));
      if (!cfg.changeGridSign) { activePower *= -1.0f; }
      r.gridWatts = activePower;

      // reactivePower (buffer[33..34]) sem campo equivalente; descartado.

      r.powerFactor = (float)((buffer[35] << 8) + buffer[36]) / 1000.0f;
      r.frequency = (float)((buffer[37] << 8) + buffer[38]) / 100.0f;

      // O DDS238-2 existe em duas variantes de firmware que colocam a
      // energia importada/exportada em blocos de registos diferentes;
      // o original tenta o primeiro bloco e só usa o segundo se ambos os
      // valores do primeiro forem zero. Mantido tal e qual.
      float exportActive = (float)((buffer[11] << 24) + (buffer[12] << 16) + (buffer[13] << 8) + buffer[14]) / 100.0f;
      float importActive = (float)((buffer[15] << 24) + (buffer[16] << 16) + (buffer[17] << 8) + buffer[18]) / 100.0f;

      if (!(importActive > 0 || exportActive > 0)) {
        exportActive = (float)((buffer[19] << 24) + (buffer[20] << 16) + (buffer[21] << 8) + buffer[22]) / 100.0f;
        importActive = (float)((buffer[23] << 24) + (buffer[24] << 16) + (buffer[25] << 8) + buffer[26]) / 100.0f;
      }

      r.importActiveEnergy = importActive;
      r.exportActiveEnergy = exportActive;

      fields_.gridVoltage = true;
      fields_.gridCurrent = true;
      fields_.gridWatts = true;
      fields_.powerFactor = true;
      fields_.frequency = true;
      fields_.importActiveEnergy = true;
      fields_.exportActiveEnergy = true;

      publishReading(r);
    }
  }

  if (sendRetry_ == 0 || dataReady) {
    sendRetry_ = 5;
    sendRequest(0x03, 0, 18);
  } else {
    sendRetry_--;
  }
}

// ---------------------------------------------------------------------------
// DDSU666 (ddsu666 original): ciclo de 8 pedidos de 2 registos (32 bits
// float), função 0x04, um registo por chamada (meter.read_state).
// ---------------------------------------------------------------------------
void ModbusRtuMeterManager::tickDdsu666() {
  static const uint16_t kAddresses[8] = {
      0x2000, // tensão            [V]
      0x2002, // corrente          [A]
      0x2004, // potência activa   [kW]
      0x2006, // potência reactiva [kVAr]
      0x200A, // factor de potência
      0x200E, // frequência        [Hz]
      0x4000, // energia importada [kWh]
      0x400A, // energia exportada [kWh]
  };

  bool dataReady = receiveReady();

  if (dataReady) {
    uint8_t buffer[14]; // >= 5 + (2 * 2) = 9
    uint8_t error = receiveBuffer(buffer, sizeof(buffer), 2);

    if (error) {
      Logger::info("ModbusRtuMeterManager (DDSU666): erro %u\n", (unsigned)error);
    } else {
      const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
      float value = decodeFloatBE(buffer, 3);
      PowerReading r = reading_;

      switch (ddsu666State_) {
        case 0:
          r.gridVoltage = value;
          fields_.gridVoltage = true;
          break;
        case 1:
          r.gridCurrent = value;
          fields_.gridCurrent = true;
          break;
        case 2: {
          float watts = value * 1000.0f;
          if (!cfg.changeGridSign) { watts *= -1.0f; }
          r.gridWatts = watts;
          fields_.gridWatts = true;
          break;
        }
        case 3:
          // potência reactiva: sem campo equivalente em PowerReading;
          // descartada (idem original, que só a usava em log de debug).
          break;
        case 4:
          r.powerFactor = value;
          fields_.powerFactor = true;
          break;
        case 5:
          r.frequency = value;
          fields_.frequency = true;
          break;
        case 6:
          r.importActiveEnergy = value;
          fields_.importActiveEnergy = true;
          break;
        case 7:
          r.exportActiveEnergy = value;
          fields_.exportActiveEnergy = true;
          break;
        default:
          break;
      }

      publishReading(r);
      ddsu666State_ = (uint8_t)((ddsu666State_ + 1) % 8);
    }
  }

  if (sendRetry_ == 0 || dataReady) {
    sendRetry_ = 5;
    sendRequest(0x04, kAddresses[ddsu666State_], 2);
  } else {
    sendRetry_--;
  }
}

// ---------------------------------------------------------------------------
// SDM120/220/230 (sdm120 original): um único pedido de 80 registos,
// função 0x04, a partir do endereço 0.
// ---------------------------------------------------------------------------
void ModbusRtuMeterManager::tickSdm120() {
  bool dataReady = receiveReady();

  if (dataReady) {
    uint8_t buffer[170];
    uint8_t error = receiveBuffer(buffer, sizeof(buffer), 80);

    if (error) {
      Logger::info("ModbusRtuMeterManager (SDM120/220/230): erro %u\n", (unsigned)error);
    } else {
      const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
      PowerReading r = reading_;

      r.gridVoltage = decodeFloatBE(buffer, 3);
      r.gridCurrent = decodeFloatBE(buffer, 15);

      float activePower = decodeFloatBE(buffer, 27);
      if (!cfg.changeGridSign) { activePower *= -1.0f; }
      r.gridWatts = activePower;

      // aparentPower (offset 39) e reactivePower (offset 51): sem campo
      // equivalente em PowerReading; descartados.

      r.powerFactor = decodeFloatBE(buffer, 63);

      // phaseAngle (offset 75): sem campo equivalente; descartado.

      r.frequency = decodeFloatBE(buffer, 143);
      r.importActiveEnergy = decodeFloatBE(buffer, 147);
      r.exportActiveEnergy = decodeFloatBE(buffer, 151);

      // importReactive/exportReactive (offsets 155/159) e energyTotal
      // (import+export, calculado no original): sem campo equivalente;
      // descartados.

      fields_.gridVoltage = true;
      fields_.gridCurrent = true;
      fields_.gridWatts = true;
      fields_.powerFactor = true;
      fields_.frequency = true;
      fields_.importActiveEnergy = true;
      fields_.exportActiveEnergy = true;

      publishReading(r);
    }
  }

  if (sendRetry_ == 0 || dataReady) {
    sendRetry_ = 5;
    sendRequest(0x04, 0x00, 80);
  } else {
    sendRetry_--;
  }
}

// ---------------------------------------------------------------------------
// MustSolar (mustSolar original): um único pedido de 75 registos, função
// 0x03, a partir do endereço 6271.
// ---------------------------------------------------------------------------
void ModbusRtuMeterManager::tickMustSolar() {
  bool dataReady = receiveReady();

  if (dataReady) {
    uint8_t buffer[160];
    uint8_t error = receiveBuffer(buffer, sizeof(buffer), 75);

    if (error) {
      Logger::info("ModbusRtuMeterManager (MustSolar): erro %u\n", (unsigned)error);
    } else {
      const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
      PowerReading r = reading_;

      r.solarWatts = (float)((buffer[24] << 8) + buffer[25]) / 10.0f;

      float gridWatts = (float)((buffer[26] << 8) + buffer[27]) / 10.0f;
      if (!cfg.changeGridSign) { gridWatts *= -1.0f; }
      r.gridWatts = gridWatts;

      fields_.solarWatts = true;
      fields_.gridWatts = true;

      publishReading(r);
    }
  }

  if (sendRetry_ == 0 || dataReady) {
    sendRetry_ = 5;
    sendRequest(0x03, 6271, 75);
  } else {
    sendRetry_--;
  }
}
