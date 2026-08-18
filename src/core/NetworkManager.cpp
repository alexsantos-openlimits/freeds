#include "NetworkManager.h"
#include "AppConfig.h"
#include "Logger.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>

namespace {
WiFiMulti g_wifiMulti;
}

int WifiGetRssiAsQuality(int rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -50) return 100;
  return 2 * (rssi + 100);
}

void NetworkManager::begin() {
  const NetworkConfig &net = ConfigStore::get().network;

  if (!net.configured || strlen(net.ssid1) == 0) {
    startAccessPoint();
    return;
  }

  connectStation();
}

void NetworkManager::startAccessPoint() {
  apMode_ = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("FreeDS");
  IPAddress myIp = WiFi.softAPIP();
  Logger::info("Rede: modo ponto de acesso, IP local: %s\n", myIp.toString().c_str());
  dnsServer_.start(53, "*", myIp);
}

void NetworkManager::connectStation() {
  const NetworkConfig &net = ConfigStore::get().network;
  apMode_ = false;

  WiFi.mode(WIFI_STA);
  if (!net.dhcp) {
    IPAddress ip, gw, mask, dns1, dns2;
    ip.fromString(net.ip);
    gw.fromString(net.gateway);
    mask.fromString(net.subnet);
    dns1.fromString(net.dns1);
    dns2.fromString(net.dns2);
    WiFi.config(ip, gw, mask, dns1, dns2);
  }

  if (strlen(net.ssid1) > 0) g_wifiMulti.addAP(net.ssid1, net.pass1);
  if (strlen(net.ssid2) > 0) g_wifiMulti.addAP(net.ssid2, net.pass2);

  g_wifiMulti.run();

  if (WiFi.status() == WL_CONNECTED) {
    Logger::info("Rede: ligado, IP: %s\n", WiFi.localIP().toString().c_str());

    if (!MDNS.begin(net.hostname)) {
      Logger::info("Rede: erro ao iniciar mDNS\n");
    }

    configTzTime(ConfigStore::get().system.timezone, ConfigStore::get().system.ntpServer);
    updateLocalTime();
  }
}

void NetworkManager::loop() {
  if (apMode_) {
    dnsServer_.processNextRequest();
    return;
  }

  if (WiFi.status() != WL_CONNECTED && millis() - lastReconnectAttemptMs_ > 5000) {
    lastReconnectAttemptMs_ = millis();
    g_wifiMulti.run();
  }
}

bool NetworkManager::isConnected() const {
  return apMode_ ? true : WiFi.status() == WL_CONNECTED;
}

void NetworkManager::updateLocalTime() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    ntpTimeOk_ = false;
    return;
  }
  ntpTimeOk_ = timeinfo.tm_year >= 120;
}

std::vector<WifiScanResult> NetworkManager::scan() {
  std::vector<WifiScanResult> results;
  int count = WiFi.scanNetworks();
  for (int i = 0; i < count; i++) {
    results.push_back({WiFi.SSID(i), WiFi.RSSI(i)});
  }
  return results;
}
