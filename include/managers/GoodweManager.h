/*
  GoodweManager.h - Gestor de excedentes para inversores GoodWe ES/EM

  Substitui sendUDPRequest()/parseUDP() de goodwe.ino e o fio condutor que
  existia em FreeDS.ino/Support_functions.ino (abertura do socket UDP no
  setup, envio periódico do pedido a partir de getSensorData() e
  processamento não bloqueante do datagrama de resposta a cada volta do
  loop()).

  Protocolo: pedido UDP fixo de 9 bytes enviado à porta 8899 do IP
  configurado (SurplusManagerConfig::sourceIp); o inversor responde com um
  datagrama binário do qual se extraem, por offset fixo, as tensões/
  correntes dos dois MPPT, dados da bateria (tensão, corrente, sinal de
  carga/descarga, SoC), tensão/potência/frequência de rede e temperatura
  do inversor. Ver GoodweManager.cpp para os offsets exatos (idênticos aos
  de goodwe.ino original).

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include <WiFiUdp.h>

#include "SurplusManager.h"

class GoodweManager : public SurplusManagerBase {
public:
  GoodweManager();

  void begin() override;
  void loop() override;
  const char *name() const override { return "GoodWe ES/EM"; }

private:
  void sendRequest();
  void processIncoming();

  WiFiUDP udp_;
  unsigned long lastRequestMs_ = 0;
  uint8_t buffer_[512];

  // Porta local de escuta e porta de destino do inversor - ambas 8899,
  // tal como no goodwe.ino original (localUdpPort / inverterUDP.beginPacket).
  static const uint16_t kLocalPort = 8899;
  static const uint16_t kInverterPort = 8899;
};
