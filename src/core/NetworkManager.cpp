#include "NetworkManager.h"
#include "AppConfig.h"
#include "Logger.h"

#include <WiFi.h>
#include <WiFiMulti.h>
#include <ESPmDNS.h>

namespace {
WiFiMulti g_wifiMulti; // só usado depois de já ter ligado uma vez (retry indefinido, alternando as duas redes)

bool debugEnabled() { return (ConfigStore::get().system.debugFlags & 0x01) != 0; }
} // namespace

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

  beginBootSequence();
}

void NetworkManager::startAccessPoint() {
  apMode_ = true;
  WiFi.mode(WIFI_AP);
  WiFi.softAP("Lusol");
  IPAddress myIp = WiFi.softAPIP();
  Logger::info("Rede: modo ponto de acesso, IP local: %s\n", myIp.toString().c_str());
  dnsServer_.start(53, "*", myIp);
}

void NetworkManager::beginBootSequence() {
  const NetworkConfig &net = ConfigStore::get().network;
  bootPhase_ = WifiBootPhase::TryingPrimary;
  phaseStartMs_ = millis();
  Logger::info("Rede: a ligar a \"%s\" (rede 1)...\n", net.ssid1);
  attemptConnect(net.ssid1, net.pass1);
}

void NetworkManager::attemptConnect(const char *ssid, const char *pass) {
  lastAttemptMs_ = millis();
  WiFi.disconnect();
  WiFi.begin(ssid, pass);
}

void NetworkManager::finalizeConnection() {
  const NetworkConfig &net = ConfigStore::get().network;
  everConnected_ = true;
  bootPhase_ = WifiBootPhase::Done;
  Logger::info("Rede: ligado, IP: %s\n", WiFi.localIP().toString().c_str());

  if (!MDNS.begin(net.hostname)) {
    Logger::info("Rede: erro ao iniciar mDNS\n");
  }

  configTzTime(ConfigStore::get().system.timezone, ConfigStore::get().system.ntpServer);
  updateLocalTime();

  // Prepara o WiFiMulti com as duas redes para, se cair depois de já ter
  // ligado, alternar entre elas indefinidamente (ver loop()).
  if (strlen(net.ssid1) > 0) g_wifiMulti.addAP(net.ssid1, net.pass1);
  if (strlen(net.ssid2) > 0) g_wifiMulti.addAP(net.ssid2, net.pass2);
}

void NetworkManager::onBootSequenceExhausted() {
  Logger::info("Rede: nao foi possivel ligar a nenhuma rede configurada, a entrar em modo ponto de acesso\n");
  startAccessPoint();
}

void NetworkManager::loop() {
  if (apMode_) {
    dnsServer_.processNextRequest();
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    if (bootPhase_ != WifiBootPhase::Done) finalizeConnection();
    return;
  }

  // Ainda na sequência de arranque (nunca ligou com sucesso): rede 1 durante
  // kPerNetworkTimeoutMs, depois rede 2 pelo mesmo tempo, e só depois AP.
  if (!everConnected_) {
    const NetworkConfig &net = ConfigStore::get().network;
    unsigned long elapsedInPhase = millis() - phaseStartMs_;

    if (bootPhase_ == WifiBootPhase::TryingPrimary && elapsedInPhase > kPerNetworkTimeoutMs) {
      if (strlen(net.ssid2) > 0) {
        bootPhase_ = WifiBootPhase::TryingSecondary;
        phaseStartMs_ = millis();
        Logger::info("Rede: rede 1 (\"%s\") nao respondeu em %lus, a tentar rede 2 (\"%s\")...\n",
                     net.ssid1, kPerNetworkTimeoutMs / 1000, net.ssid2);
        attemptConnect(net.ssid2, net.pass2);
      } else {
        onBootSequenceExhausted();
      }
      return;
    }

    if (bootPhase_ == WifiBootPhase::TryingSecondary && elapsedInPhase > kPerNetworkTimeoutMs) {
      onBootSequenceExhausted();
      return;
    }

    // Dentro da mesma janela, repete a tentativa de vez em quando (o
    // primeiro WiFi.begin() pode falhar rápido, ex.: password errada).
    if (millis() - lastAttemptMs_ > kAttemptRetryMs) {
      if (debugEnabled()) {
        Logger::info("Rede: ainda sem ligacao (status wifi %d), a tentar de novo...\n", (int)WiFi.status());
      }
      const char *ssid = bootPhase_ == WifiBootPhase::TryingPrimary ? net.ssid1 : net.ssid2;
      const char *pass = bootPhase_ == WifiBootPhase::TryingPrimary ? net.pass1 : net.pass2;
      attemptConnect(ssid, pass);
    }
    return;
  }

  // Já ligou pelo menos uma vez: continua a tentar as duas redes
  // indefinidamente, sem voltar a modo AP por conta própria.
  if (millis() - lastReconnectAttemptMs_ > 5000) {
    lastReconnectAttemptMs_ = millis();
    if (debugEnabled()) Logger::info("Rede: ligacao perdida, a tentar religar...\n");
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
