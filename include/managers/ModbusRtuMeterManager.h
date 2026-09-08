/*
  ModbusRtuMeterManager.h - Gestor de excedentes para contadores Modbus RTU
  (RS485) ligados à UART1 do ESP32

  Substitui modbus.ino + modbus_functions.ino: constrói as tramas Modbus RTU
  manualmente (não existe biblioteca Modbus RTU vendorizada no projeto),
  envia o pedido para o contador configurado e, em chamadas seguintes de
  loop(), lê a resposta com um timeout curto (não bloqueante). Cada tipo de
  contador tem o seu próprio mapa de registos/escala, copiado tal e qual do
  ficheiro original:

  - DDS238_METER  (dds2382): um único pedido de 18 registos (função 0x03).
  - DDSU666_METER (ddsu666): ciclo de 8 pedidos de 2 registos (função 0x04),
    um registo (tensão/corrente/potência/...) por chamada, tal como o
    original (meter.read_state).
  - SDM_METER     (sdm120):  um único pedido de 80 registos (função 0x04).
  - MUSTSOLAR     (mustSolar): um único pedido de 75 registos (função 0x03)
    a partir do endereço 6271.
*/
#pragma once

#include <HardwareSerial.h>

#include "SurplusManager.h"

class ModbusRtuMeterManager : public SurplusManagerBase {
public:
  ModbusRtuMeterManager();

  void begin() override;
  void loop() override;
  const char *name() const override;

private:
  // UART1 do ESP32, tal como Lusol.ino original (RX1/TX1).
  static const int8_t kRxPin = 19;
  static const int8_t kTxPin = 23;
  static const uint32_t kReceiveTimeoutMs = 10; // igual ao original (modbusReceiveBuffer)

  // O DDSU666 lê um registo por passo; são precisos 8 passos para um ciclo
  // completo (ver loop()). O mínimo por passo dá tempo à resposta do
  // contador e evita saturar o barramento RS485 com pedidos.
  static const uint8_t kDdsu666States = 8;
  static const uint32_t kMinStepIntervalMs = 60;

  HardwareSerial serial_;

  uint8_t lastMode_ = 0;
  uint32_t lastBaud_ = 0;
  uint32_t lastTickMs_ = 0;
  uint8_t sendRetry_ = 0;
  uint8_t ddsu666State_ = 0;
  uint8_t expectedAddress_ = 0; // endereço do escravo à espera de resposta

  void tickDds2382();
  void tickDdsu666();
  void tickSdm120();
  void tickMustSolar();

  void sendRequest(uint8_t functionCode, uint16_t startAddress, uint16_t registerCount);
  // Não pode ser const: HardwareSerial::available() não é const neste core
  // do Arduino-ESP32, e chamá-lo sobre serial_ dentro de um método const
  // tornaria serial_ implicitamente const, o que não compila (-fpermissive).
  bool receiveReady();

  // Devolve 0 em sucesso; caso contrário, o código de erro (7 = dados
  // insuficientes, 9 = CRC inválido, ou o código de excepção Modbus
  // devolvido pelo escravo), tal como modbusReceiveBuffer() original.
  uint8_t receiveBuffer(uint8_t *buffer, uint8_t bufferSize, uint8_t registerCount);

  static uint16_t calculateCrc(const uint8_t *frame, uint8_t len);
  static float decodeFloatBE(const uint8_t *data, uint8_t offset);
};
