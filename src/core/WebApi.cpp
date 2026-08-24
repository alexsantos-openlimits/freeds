/*
  WebApi.cpp - ver WebApi.h

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
*/
#include "WebApi.h"
#include "AppConfig.h"
#include "Logger.h"
#include "EnergyTracker.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <Update.h>
#include <WiFi.h>

extern "C" {
#include <crypto/base64.h>
}

namespace {

// String::concat(const char*, unsigned int) é protected nalgumas versões do
// core Arduino-ESP32 (não é garantido ser acessível de fora da classe); esta
// função constrói a String a partir de um buffer não necessariamente
// terminado em '\0' sem depender dessa sobrecarga.
String bytesToString(const char *data, size_t len) {
  String result;
  result.reserve(len);
  for (size_t i = 0; i < len; i++) result += data[i];
  return result;
}

String decodeBase64(const char *b64) {
  size_t outLen = 0;
  unsigned char *out = base64_decode((const unsigned char *)b64, strlen(b64), &outLen);
  if (!out) return "";
  String result = bytesToString((const char *)out, outLen);
  free(out);
  return result;
}

bool requireAuth(AsyncWebServerRequest *request) {
  String password = decodeBase64(ConfigStore::get().system.adminPasswordB64);
  if (request->authenticate("admin", password.c_str())) return true;
  request->requestAuthentication();
  return false;
}

const char *workingModeToString(LoadWorkingMode m) {
  switch (m) {
    case LoadWorkingMode::Manual: return "manual";
    case LoadWorkingMode::Off: return "off";
    default: return "auto";
  }
}

} // namespace

void WebApi::begin(LoadController *load, TemperatureManager *temperature, NetworkManager *network,
                    SurplusManager **surplus, CurrentClampSensor *clamp, void (*onSurplusModeChanged)()) {
  load_ = load;
  temperature_ = temperature;
  network_ = network;
  surplus_ = surplus;
  clamp_ = clamp;
  onSurplusModeChanged_ = onSurplusModeChanged;

  server_.addHandler(&events_);
  Logger::setWeblogSink([this](const char *line) { events_.send(line, "weblog"); });

  registerApiRoutes();
  registerSystemRoutes();

  server_.serveStatic("/i18n/", SPIFFS, "/i18n/");
  server_.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");
  server_.onNotFound([](AsyncWebServerRequest *request) { request->send(404, "text/plain", "Not found"); });

  server_.begin();
}

String WebApi::buildStatusJson() {
  DynamicJsonDocument doc(2048);

  const SurplusManager *sm = (surplus_ && *surplus_) ? *surplus_ : nullptr;
  static const PowerReading emptyReading;
  static const PowerReadingFields emptyFields;
  const PowerReading &r = sm ? sm->reading() : emptyReading;
  const PowerReadingFields &f = sm ? sm->fields() : emptyFields;

  doc["workingMode"] = workingModeToString(load_->workingMode());
  doc["pwmPercent"] = load_->pwmPercent();

  JsonObject grid = doc.createNestedObject("grid");
  grid["watts"] = r.gridWatts;
  grid["voltage"] = r.gridVoltage;
  grid["current"] = r.gridCurrent;
  grid["frequency"] = r.frequency;
  grid["powerFactor"] = r.powerFactor;

  JsonObject solar = doc.createNestedObject("solar");
  solar["watts"] = r.solarWatts;
  solar["today"] = r.solarWattsToday;

  JsonObject battery = doc.createNestedObject("battery");
  battery["watts"] = r.batteryWatts;
  battery["soc"] = r.batterySoc;

  JsonObject pv1 = doc.createNestedObject("pv1");
  pv1["voltage"] = r.pv1Voltage;
  pv1["current"] = r.pv1Current;
  pv1["watts"] = r.pv1Watts;
  JsonObject pv2 = doc.createNestedObject("pv2");
  pv2["voltage"] = r.pv2Voltage;
  pv2["current"] = r.pv2Current;
  pv2["watts"] = r.pv2Watts;

  doc["inverterTemperature"] = r.inverterTemperature;
  // Quando a fonte de dados não reporta o consumo da carga, usa a
  // estimativa da pinça amperimétrica (ou a curva teórica do dimmer),
  // tal como o antigo readClamp()/currentCalcWatts.
  bool loadWattsFromSource = f.loadWatts;
  doc["loadWatts"] = loadWattsFromSource ? r.loadWatts : (clamp_ ? clamp_->lastWatts() : 0);

  JsonArray relays = doc.createNestedArray("relays");
  for (uint8_t i = 0; i < 4; i++) {
    JsonObject relay = relays.createNestedObject();
    relay["on"] = load_->relayState(i).energized;
    relay["auto"] = load_->relayState(i).autoActivated;
    relay["manual"] = load_->relayIsManual(i);
  }

  JsonObject temps = doc.createNestedObject("temperatures");
  temps["enabled"] = ConfigStore::get().temperature.enabled;
  temps["thermo"] = temperature_->thermoTemperature();
  temps["triac"] = temperature_->triacTemperature();
  temps["custom"] = temperature_->customTemperature();

  const EnergyTotals &e = ConfigStore::get().energy;
  JsonObject energyToday = doc.createNestedObject("energyToday");
  energyToday["importedKwh"] = e.kwToday;
  energyToday["exportedKwh"] = e.kwExportToday;
  JsonObject energyTotal = doc.createNestedObject("energyTotal");
  energyTotal["importedKwh"] = e.kwTotal;
  energyTotal["exportedKwh"] = e.kwExportTotal;

  JsonObject wifi = doc.createNestedObject("wifi");
  wifi["connected"] = WiFi.status() == WL_CONNECTED;
  wifi["ssid"] = WiFi.SSID();
  wifi["rssi"] = WiFi.RSSI();
  wifi["ip"] = WiFi.localIP().toString();

  doc["sourceConnected"] = sm ? sm->isConnected() : false;
  doc["dataFault"] = load_->hasDataFault();
  doc["uptimeSeconds"] = millis() / 1000;

  JsonObject fields = doc.createNestedObject("fields");
  fields["gridWatts"] = f.gridWatts;
  fields["solarWatts"] = f.solarWatts;
  fields["solarWattsToday"] = f.solarWattsToday;
  fields["gridVoltage"] = f.gridVoltage;
  fields["gridCurrent"] = f.gridCurrent;
  fields["frequency"] = f.frequency;
  fields["powerFactor"] = f.powerFactor;
  fields["pv1"] = f.pv1;
  fields["pv2"] = f.pv2;
  fields["inverterTemperature"] = f.inverterTemperature;
  fields["batteryWatts"] = f.batteryWatts;
  fields["batterySoc"] = f.batterySoc;
  fields["loadWatts"] = f.loadWatts || clamp_ != nullptr;

  String out;
  serializeJson(doc, out);
  return out;
}

void WebApi::handleConfigSection(AsyncWebServerRequest *request, uint8_t *data, size_t len, const char *section) {
  if (!requireAuth(request)) return;

  String body = bytesToString((const char *)data, len);

  String wrapped = String("{\"") + section + "\":" + body + "}";
  bool ok = ConfigStore::importJson(wrapped);

  if (!ok) {
    request->send(400, "application/json", "{\"error\":\"json invalido\"}");
    return;
  }

  if (strcmp(section, "surplus") == 0 && onSurplusModeChanged_) {
    onSurplusModeChanged_();
  }

  request->send(200, "application/json", ConfigStore::exportJson());
}

void WebApi::registerApiRoutes() {
  server_.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) {
    request->send(200, "application/json", buildStatusJson());
  });

  server_.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", ConfigStore::exportJson());
  });

  auto configHandler = [this](const char *section) {
    return [this, section](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        // corpo pode chegar em vários pedaços; para simplificar assumimos
        // que a configuração cabe sempre num único pedaço (poucos KB).
      }
      if (index + len == total) {
        handleConfigSection(request, data, len, section);
      }
    };
  };

  server_.on("/api/config/network", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr, configHandler("network"));
  server_.on("/api/config/mqtt", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr, configHandler("mqtt"));
  server_.on("/api/config/surplus-manager", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr, configHandler("surplus"));
  server_.on("/api/config/load", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr, configHandler("load"));
  server_.on("/api/config/temperature", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr, configHandler("temperature"));
  server_.on("/api/config/system", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr, configHandler("system"));

  server_.on("/api/control", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr,
             [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
               if (index + len != total) return;
               DynamicJsonDocument doc(512);
               if (deserializeJson(doc, data, len)) {
                 request->send(400);
                 return;
               }

               if (doc.containsKey("workingMode")) {
                 String mode = doc["workingMode"].as<String>();
                 if (mode == "auto") load_->setWorkingMode(LoadWorkingMode::Auto);
                 else if (mode == "manual") load_->setWorkingMode(LoadWorkingMode::Manual);
                 else if (mode == "off") load_->setWorkingMode(LoadWorkingMode::Off);
               }

               if (doc.containsKey("manualPwmPercent")) {
                 ConfigStore::get().load.manualControlPercent = doc["manualPwmPercent"].as<uint16_t>();
                 ConfigStore::save();
               }

               if (doc.containsKey("relay")) {
                 uint8_t idx = doc["relay"]["index"].as<uint8_t>();
                 String relayMode = doc["relay"]["mode"].as<String>();
                 if (idx < 4) {
                   if (relayMode == "on") load_->setRelayManual(idx, true);
                   else if (relayMode == "off") load_->setRelayManual(idx, false);
                   else load_->setRelayAuto(idx);
                 }
               }

               request->send(200, "application/json", buildStatusJson());
             });

  server_.on("/api/temperature/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    JsonArray sensors = doc.createNestedArray("sensors");
    for (uint8_t i = 0; i < temperature_->sensorCount(); i++) {
      const uint8_t *addr = temperature_->sensorAddress(i);
      char hex[17];
      for (uint8_t b = 0; b < 8; b++) sprintf(hex + b * 2, "%02X", addr[b]);
      sensors.add(hex);
    }
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  server_.on("/api/wifi/scan", HTTP_POST, [this](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    JsonArray networks = doc.createNestedArray("networks");
    for (auto &r : network_->scan()) {
      JsonObject n = networks.createNestedObject();
      n["ssid"] = r.ssid;
      n["rssi"] = r.rssi;
    }
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });
}

void WebApi::registerSystemRoutes() {
  server_.on("/api/backup", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", ConfigStore::exportJson());
    response->addHeader("Content-Disposition", "attachment; filename=\"freeds-backup.json\"");
    request->send(response);
  });

  server_.on("/api/restore", HTTP_POST, [](AsyncWebServerRequest *) {}, nullptr,
             [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
               if (!requireAuth(request)) return;
               if (index + len != total) return;
               String body = bytesToString((const char *)data, len);
               if (!ConfigStore::importJson(body)) {
                 request->send(400, "application/json", "{\"error\":\"json invalido\"}");
                 return;
               }
               request->send(200);
               restartSoon();
             });

  server_.on("/api/factory-reset", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    ConfigStore::resetToDefaults();
    request->send(200);
    restartSoon();
  });

  server_.on("/api/system/restart", HTTP_POST, [this](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    request->send(200);
    restartSoon();
  });

  // Alias simples para reiniciar sem repor de fábrica (usado pela SPA).
  server_.on("/reboot", HTTP_GET, [this](AsyncWebServerRequest *request) {
    if (!requireAuth(request)) return;
    request->send(200);
    restartSoon();
  });

  server_.on(
      "/update", HTTP_POST,
      [](AsyncWebServerRequest *request) { request->send(Update.hasError() ? 500 : 200); },
      [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
        if (!requireAuth(request)) return;

        if (!index) {
          const char *dot = strrchr(filename.c_str(), '.');
          if (!dot || strcmp(dot + 1, "bin") != 0) {
            Update.end();
            request->send(500, "text/plain", "Extensao nao suportada, e necessario um ficheiro .bin");
            return;
          }
          Logger::info("Atualizacao: a iniciar (%s)\n", filename.c_str());
          load_->shutdown(true, "PWM: a desligar para atualizacao\n");

          if (filename == "spiffs.bin") {
            SPIFFS.end();
            if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) Update.printError(Serial);
          } else {
            if (!Update.begin((ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000)) Update.printError(Serial);
          }
        }

        if (Update.hasError()) return;

        if (Update.write(data, len) != len) {
          Update.printError(Serial);
          return;
        }

        if (final) {
          if (Update.end(true)) {
            Logger::info("Atualizacao: sucesso (%uB)\n", (unsigned)(index + len));
            delay(500);
            ESP.restart();
          } else {
            Update.printError(Serial);
          }
        }
      });
}

void WebApi::restartSoon() {
  delay(300); // dá tempo à resposta HTTP para ser enviada antes de reiniciar
  ESP.restart();
}

void WebApi::loop() {
  if (millis() - lastStatusPushMs_ > 1500) {
    lastStatusPushMs_ = millis();
    events_.send(buildStatusJson().c_str(), "status");
  }
}
