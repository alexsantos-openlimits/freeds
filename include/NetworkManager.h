/*
  NetworkManager.h - Ligação Wi-Fi (estação + ponto de acesso de
  configuração inicial), mDNS e hora NTP.

  Substitui a parte de ligação Wi-Fi/portal cativo do antigo FreeDS.ino.

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
  void connectStation();

  bool apMode_ = false;
  bool ntpTimeOk_ = false;
  unsigned long lastReconnectAttemptMs_ = 0;
  DNSServer dnsServer_;
};

int WifiGetRssiAsQuality(int rssi);
