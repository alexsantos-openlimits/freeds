/*
  NetworkManager.h - Ligação Wi-Fi (estação + ponto de acesso de
  configuração inicial), mDNS e hora NTP.

  Substitui a parte de ligação Wi-Fi/portal cativo do antigo FreeDS.ino.

  Sequência de arranque (quando já há rede configurada): tenta a rede 1
  durante kPerNetworkTimeoutMs; se não conseguir, tenta a rede 2 pelo mesmo
  tempo; se também falhar, cai em modo ponto de acesso para o utilizador
  poder corrigir a configuração. Depois de alguma vez ter ligado com
  sucesso, uma queda de rede já não volta ao modo AP automaticamente -
  fica a tentar as duas redes indefinidamente (para não deixar o
  dispositivo "às escuras" só por uma falha momentânea do router).

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#pragma once

#include <Arduino.h>
#include <DNSServer.h>
#include <vector>

struct WifiScanResult {
  String ssid;
  int32_t rssi;
};

enum class WifiBootPhase {
  TryingPrimary,
  TryingSecondary,
  Done, // ligado com sucesso pelo menos uma vez, ou não há sequência a correr
};

class NetworkManager {
public:
  void begin();
  void loop();

  bool isConnected() const;
  bool isAccessPointMode() const { return apMode_; }
  DNSServer *dnsServer() { return &dnsServer_; }

  std::vector<WifiScanResult> scan();
  void updateLocalTime();
  bool hasNtpTime() const { return ntpTimeOk_; }

private:
  void startAccessPoint();
  void beginBootSequence();
  void attemptConnect(const char *ssid, const char *pass);
  void finalizeConnection();
  void onBootSequenceExhausted();

  bool apMode_ = false;
  bool ntpTimeOk_ = false;
  bool everConnected_ = false;
  WifiBootPhase bootPhase_ = WifiBootPhase::Done;
  unsigned long phaseStartMs_ = 0;
  unsigned long lastAttemptMs_ = 0;
  unsigned long lastReconnectAttemptMs_ = 0;
  DNSServer dnsServer_;

  static const unsigned long kPerNetworkTimeoutMs = 90000; // 1m30s por rede no arranque
  static const unsigned long kAttemptRetryMs = 8000;
};

int WifiGetRssiAsQuality(int rssi);
