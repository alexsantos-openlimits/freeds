/*
  GoodweManager.cpp - ver GoodweManager.h
*/
#include "managers/GoodweManager.h"

#include "AppConfig.h"
#include "Logger.h"

GoodweManager::GoodweManager() {}

void GoodweManager::begin() {
  udp_.begin(kLocalPort);
  Logger::info("GoodweManager: a escutar respostas UDP na porta %u\n", (unsigned)kLocalPort);
}

void GoodweManager::loop() {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
  unsigned long interval = cfg.pollIntervalMs > 0 ? cfg.pollIntervalMs : 1000;

  if (lastRequestMs_ == 0 || (millis() - lastRequestMs_) >= interval) {
    lastRequestMs_ = millis();
    sendRequest();
  }

  // Não bloqueante: só processa se já houver um datagrama completo em fila.
  processIncoming();
}

void GoodweManager::sendRequest() {
  const char *ip = ConfigStore::get().surplus.sourceIp;
  if (ip[0] == '\0') { return; }

  udp_.beginPacket(ip, kInverterPort);
  udp_.write(0xAA);
  udp_.write(0x55);
  udp_.write(0xC0);
  udp_.write(0x7F);
  udp_.write(0x01);
  udp_.write(0x06);
  udp_.write(0x00);
  udp_.write(0x02);
  udp_.write(0x45);
  udp_.endPacket();
}

void GoodweManager::processIncoming() {
  int packetSize = udp_.parsePacket();
  if (packetSize <= 0) { return; }

  // sizeof(buffer_) - 1 para deixar sempre espaço para o terminador nulo
  // (o goodwe.ino original lia até 512 bytes num buffer de 512 e só depois
  // escrevia o terminador, o que podia escrever 1 byte fora dos limites
  // num datagrama do tamanho máximo; aqui isso fica corrigido).
  int len = udp_.read(buffer_, sizeof(buffer_) - 1);
  if (len <= 0) { return; }
  buffer_[len] = 0;

  // O maior offset lido abaixo é buffer_[87]; um datagrama mais curto do
  // que isso não é uma resposta GoodWe válida (ex.: pacote truncado ou de
  // outro dispositivo a chegar à mesma porta UDP) - descarta-se aqui em vez
  // de ler memória não inicializada e publicar uma leitura sem sentido
  // como se fosse válida.
  if (len < 88) {
    Logger::info("GoodweManager: datagrama demasiado curto (%d bytes), descartado\n", len);
    return;
  }

  int16_t value = 0;
  uint16_t uvalue = 0;
  PowerReading r = reading_;

  // PV1 Volts/Amps/Power
  uvalue = (buffer_[7] << 8) | buffer_[8];
  r.pv1Voltage = (float)uvalue / 10.0f;
  uvalue = (buffer_[9] << 8) | buffer_[10];
  r.pv1Current = (float)uvalue / 10.0f;
  r.pv1Watts = r.pv1Voltage * r.pv1Current;

  // PV2 Volts/Amps/Power
  uvalue = (buffer_[12] << 8) | buffer_[13];
  r.pv2Voltage = (float)uvalue / 10.0f;
  uvalue = (buffer_[14] << 8) | buffer_[15];
  r.pv2Current = (float)uvalue / 10.0f;
  r.pv2Watts = r.pv2Voltage * r.pv2Current;

  // PV Total Power
  r.solarWatts = r.pv1Watts + r.pv2Watts;

  // Battery Volts/Amps (uso interno; sem campo próprio em PowerReading)
  uvalue = (buffer_[17] << 8) | buffer_[18];
  float batteryVoltage = (float)uvalue / 10.0f;
  value = (buffer_[25] << 8) | buffer_[26];
  float batteryCurrent = (float)value / 10.0f;

  // Battery Power
  r.batteryWatts = batteryVoltage * batteryCurrent;
  uvalue = buffer_[37];
  if (uvalue == 3) { r.batteryWatts *= -1.0f; }

  // Battery SoC
  uvalue = (buffer_[33] << 8) | buffer_[34];
  r.batterySoc = (float)uvalue;

  // Grid Voltage
  uvalue = (buffer_[41] << 8) | buffer_[42];
  r.gridVoltage = (float)uvalue / 10.0f;

  // Grid Power
  value = (buffer_[45] << 8) | buffer_[46]; // pgrid
  r.gridWatts = (float)value;

  uvalue = buffer_[87];
  if (uvalue == 2) { r.gridWatts *= -1.0f; }

  // House Consumption (calculado antes de aplicar changeGridSign, tal como
  // no original)
  r.loadWatts = r.solarWatts + r.batteryWatts - r.gridWatts;

  if (ConfigStore::get().surplus.changeGridSign) { r.gridWatts *= -1.0f; }

  // AC Frequency
  uvalue = (buffer_[47] << 8) | buffer_[48];
  r.frequency = (float)uvalue / 100.0f;

  // e_day (energia solar diária)
  uvalue = (buffer_[74] << 8) | buffer_[75];
  r.solarWattsToday = (float)uvalue / 10.0f;

  // Inverter Temperature
  uvalue = (buffer_[60] << 8) | buffer_[61];
  r.inverterTemperature = (float)uvalue / 10.0f;

  fields_.gridWatts = true;
  fields_.gridVoltage = true;
  fields_.frequency = true;
  fields_.solarWatts = true;
  fields_.solarWattsToday = true;
  fields_.pv1 = true;
  fields_.pv2 = true;
  fields_.inverterTemperature = true;
  fields_.batteryWatts = true;
  fields_.batterySoc = true;
  fields_.loadWatts = true;

  publishReading(r);
}
