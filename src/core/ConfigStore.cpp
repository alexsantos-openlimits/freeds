/*
  ConfigStore.cpp - Persistência da configuração em "/config.json" (SPIFFS)
*/
#include "AppConfig.h"
#include "Logger.h"

#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <WiFi.h>

static const char *CONFIG_PATH = "/config.json";

static AppConfig g_config;

AppConfig &ConfigStore::get() { return g_config; }

void ConfigStore::applyDefaults(AppConfig &cfg) {
  cfg = AppConfig(); // reaplica todos os valores por omissão definidos em AppConfig.h

  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(cfg.network.hostname, sizeof(cfg.network.hostname), "lusol_%02x%02x", mac[4], mac[5]);

  snprintf(cfg.mqtt.relayTopic[0], sizeof(cfg.mqtt.relayTopic[0]), "%s/relay/1/STATUS", cfg.network.hostname);
  snprintf(cfg.mqtt.relayTopic[1], sizeof(cfg.mqtt.relayTopic[1]), "%s/relay/2/STATUS", cfg.network.hostname);
  snprintf(cfg.mqtt.relayTopic[2], sizeof(cfg.mqtt.relayTopic[2]), "%s/relay/3/STATUS", cfg.network.hostname);
  snprintf(cfg.mqtt.relayTopic[3], sizeof(cfg.mqtt.relayTopic[3]), "%s/relay/4/STATUS", cfg.network.hostname);
}

namespace {

template <size_t N>
void copyStr(const JsonVariantConst &src, char (&dst)[N]) {
  if (!src.isNull()) {
    strlcpy(dst, src.as<const char *>(), N);
  }
}

void toJson(const AppConfig &cfg, JsonDocument &doc) {
  doc["schemaVersion"] = cfg.schemaVersion;

  JsonObject net = doc.createNestedObject("network");
  net["dhcp"] = cfg.network.dhcp;
  net["ip"] = cfg.network.ip;
  net["gateway"] = cfg.network.gateway;
  net["subnet"] = cfg.network.subnet;
  net["dns1"] = cfg.network.dns1;
  net["dns2"] = cfg.network.dns2;
  net["ssid1"] = cfg.network.ssid1;
  net["pass1"] = cfg.network.pass1;
  net["ssid2"] = cfg.network.ssid2;
  net["pass2"] = cfg.network.pass2;
  net["hostname"] = cfg.network.hostname;
  net["configured"] = cfg.network.configured;

  JsonObject mqtt = doc.createNestedObject("mqtt");
  mqtt["enabled"] = cfg.mqtt.enabled;
  mqtt["broker"] = cfg.mqtt.broker;
  mqtt["port"] = cfg.mqtt.port;
  mqtt["user"] = cfg.mqtt.user;
  mqtt["password"] = cfg.mqtt.password;
  mqtt["publishIntervalMs"] = cfg.mqtt.publishIntervalMs;
  JsonArray relayTopics = mqtt.createNestedArray("relayTopic");
  for (auto &t : cfg.mqtt.relayTopic) relayTopics.add(t);
  mqtt["solaxTopic"] = cfg.mqtt.solaxTopic;
  mqtt["meterTopic"] = cfg.mqtt.meterTopic;
  mqtt["socTopic"] = cfg.mqtt.socTopic;

  JsonObject surplus = doc.createNestedObject("surplus");
  surplus["mode"] = cfg.surplus.mode;
  surplus["sourceIp"] = cfg.surplus.sourceIp;
  surplus["esp01Ssid"] = cfg.surplus.esp01Ssid;
  surplus["esp01Password"] = cfg.surplus.esp01Password;
  surplus["meterBaud"] = cfg.surplus.meterBaud;
  surplus["meterId"] = cfg.surplus.meterId;
  surplus["solaxVersion"] = cfg.surplus.solaxVersion;
  surplus["changeGridSign"] = cfg.surplus.changeGridSign;
  surplus["useExternalMeter"] = cfg.surplus.useExternalMeter;
  surplus["useClamp"] = cfg.surplus.useClamp;
  surplus["clampCalibration"] = cfg.surplus.clampCalibration;
  surplus["clampVoltage"] = cfg.surplus.clampVoltage;
  surplus["attachedLoadWatts"] = cfg.surplus.attachedLoadWatts;
  surplus["useSolarAsMptt"] = cfg.surplus.useSolarAsMptt;
  surplus["useBmv"] = cfg.surplus.useBmv;
  surplus["gridPhase"] = cfg.surplus.gridPhase;
  surplus["maxErrorTimeMs"] = cfg.surplus.maxErrorTimeMs;
  surplus["pollIntervalMs"] = cfg.surplus.pollIntervalMs;
  surplus["pwmSlaveOnPercent"] = cfg.surplus.pwmSlaveOnPercent;

  JsonObject load = doc.createNestedObject("load");
  load["pwmEnabled"] = cfg.load.pwmEnabled;
  load["manualMode"] = cfg.load.manualMode;
  load["potTarget"] = cfg.load.potTarget;
  load["manualControlPercent"] = cfg.load.manualControlPercent;
  load["autoControlPercent"] = cfg.load.autoControlPercent;
  load["pwmFrequencyHz"] = cfg.load.pwmFrequencyHz;
  load["dimmerLowCost"] = cfg.load.dimmerLowCost;
  load["maxPwmLowCost"] = cfg.load.maxPwmLowCost;
  load["maxWattsTariff"] = cfg.load.maxWattsTariff;
  JsonObject pid = load.createNestedObject("pid");
  pid["kp"] = cfg.load.pid.kp;
  pid["ki"] = cfg.load.pid.ki;
  pid["kd"] = cfg.load.pid.kd;
  JsonArray outputs = load.createNestedArray("outputs");
  for (auto &o : cfg.load.outputs) {
    JsonObject jo = outputs.createNestedObject();
    jo["minPercent"] = o.minPercent;
    jo["onWatts"] = o.onWatts;
    jo["offWatts"] = o.offWatts;
    jo["manual"] = o.manual;
  }
  JsonObject offGrid = load.createNestedObject("offGrid");
  offGrid["enabled"] = cfg.load.offGrid.enabled;
  offGrid["useVoltage"] = cfg.load.offGrid.useVoltage;
  offGrid["socThreshold"] = cfg.load.offGrid.socThreshold;
  offGrid["battWattsThreshold"] = cfg.load.offGrid.battWattsThreshold;
  offGrid["batteryVoltage"] = cfg.load.offGrid.batteryVoltage;
  offGrid["voltageOffset"] = cfg.load.offGrid.voltageOffset;
  load["potManPwmActive"] = cfg.load.potManPwmActive;
  load["potManPwmWatts"] = cfg.load.potManPwmWatts;
  load["timerEnabled"] = cfg.load.timerEnabled;
  load["timerStartHHMM"] = cfg.load.timerStartHHMM;
  load["timerStopHHMM"] = cfg.load.timerStopHHMM;

  JsonObject temp = doc.createNestedObject("temperature");
  temp["enabled"] = cfg.temperature.enabled;
  temp["turnOnC"] = cfg.temperature.turnOnC;
  temp["turnOffC"] = cfg.temperature.turnOffC;
  temp["mode"] = cfg.temperature.mode;
  JsonArray thermoAddr = temp.createNestedArray("thermoSensorAddr");
  for (auto b : cfg.temperature.thermoSensorAddr) thermoAddr.add(b);
  JsonArray triacAddr = temp.createNestedArray("triacSensorAddr");
  for (auto b : cfg.temperature.triacSensorAddr) triacAddr.add(b);
  JsonArray customAddr = temp.createNestedArray("customSensorAddr");
  for (auto b : cfg.temperature.customSensorAddr) customAddr.add(b);
  temp["customSensorName"] = cfg.temperature.customSensorName;

  JsonObject sys = doc.createNestedObject("system");
  sys["language"] = cfg.system.language;
  sys["timezone"] = cfg.system.timezone;
  sys["ntpServer"] = cfg.system.ntpServer;
  sys["oledPower"] = cfg.system.oledPower;
  sys["oledAutoOff"] = cfg.system.oledAutoOff;
  sys["flipScreen"] = cfg.system.flipScreen;
  sys["oledAutoOffMs"] = cfg.system.oledAutoOffMs;
  sys["oledBrightness"] = cfg.system.oledBrightness;
  sys["alexaControl"] = cfg.system.alexaControl;
  sys["domoticzEnabled"] = cfg.system.domoticzEnabled;
  JsonArray idx = sys.createNestedArray("domoticzIdx");
  for (auto v : cfg.system.domoticzIdx) idx.add(v);
  sys["weblogEnabled"] = cfg.system.weblogEnabled;
  sys["serialLogEnabled"] = cfg.system.serialLogEnabled;
  sys["debugFlags"] = cfg.system.debugFlags;
  sys["adminPasswordB64"] = cfg.system.adminPasswordB64;

  JsonObject energy = doc.createNestedObject("energy");
  energy["kwToday"] = cfg.energy.kwToday;
  energy["kwExportToday"] = cfg.energy.kwExportToday;
  energy["kwYesterday"] = cfg.energy.kwYesterday;
  energy["kwExportYesterday"] = cfg.energy.kwExportYesterday;
  energy["kwTotal"] = cfg.energy.kwTotal;
  energy["kwExportTotal"] = cfg.energy.kwExportTotal;
}

void fromJson(const JsonDocument &doc, AppConfig &cfg) {
  JsonObjectConst net = doc["network"];
  if (!net.isNull()) {
    cfg.network.dhcp = net["dhcp"] | cfg.network.dhcp;
    copyStr(net["ip"], cfg.network.ip);
    copyStr(net["gateway"], cfg.network.gateway);
    copyStr(net["subnet"], cfg.network.subnet);
    copyStr(net["dns1"], cfg.network.dns1);
    copyStr(net["dns2"], cfg.network.dns2);
    copyStr(net["ssid1"], cfg.network.ssid1);
    copyStr(net["pass1"], cfg.network.pass1);
    copyStr(net["ssid2"], cfg.network.ssid2);
    copyStr(net["pass2"], cfg.network.pass2);
    copyStr(net["hostname"], cfg.network.hostname);
    cfg.network.configured = net["configured"] | cfg.network.configured;
  }

  JsonObjectConst mqtt = doc["mqtt"];
  if (!mqtt.isNull()) {
    cfg.mqtt.enabled = mqtt["enabled"] | cfg.mqtt.enabled;
    copyStr(mqtt["broker"], cfg.mqtt.broker);
    cfg.mqtt.port = mqtt["port"] | cfg.mqtt.port;
    copyStr(mqtt["user"], cfg.mqtt.user);
    copyStr(mqtt["password"], cfg.mqtt.password);
    cfg.mqtt.publishIntervalMs = mqtt["publishIntervalMs"] | cfg.mqtt.publishIntervalMs;
    JsonArrayConst relayTopics = mqtt["relayTopic"];
    if (!relayTopics.isNull()) {
      uint8_t i = 0;
      for (JsonVariantConst t : relayTopics) {
        if (i >= 4) break;
        copyStr(t, cfg.mqtt.relayTopic[i++]);
      }
    }
    copyStr(mqtt["solaxTopic"], cfg.mqtt.solaxTopic);
    copyStr(mqtt["meterTopic"], cfg.mqtt.meterTopic);
    copyStr(mqtt["socTopic"], cfg.mqtt.socTopic);
  }

  JsonObjectConst surplus = doc["surplus"];
  if (!surplus.isNull()) {
    cfg.surplus.mode = surplus["mode"] | cfg.surplus.mode;
    copyStr(surplus["sourceIp"], cfg.surplus.sourceIp);
    copyStr(surplus["esp01Ssid"], cfg.surplus.esp01Ssid);
    copyStr(surplus["esp01Password"], cfg.surplus.esp01Password);
    cfg.surplus.meterBaud = surplus["meterBaud"] | cfg.surplus.meterBaud;
    cfg.surplus.meterId = surplus["meterId"] | cfg.surplus.meterId;
    cfg.surplus.solaxVersion = surplus["solaxVersion"] | cfg.surplus.solaxVersion;
    cfg.surplus.changeGridSign = surplus["changeGridSign"] | cfg.surplus.changeGridSign;
    cfg.surplus.useExternalMeter = surplus["useExternalMeter"] | cfg.surplus.useExternalMeter;
    cfg.surplus.useClamp = surplus["useClamp"] | cfg.surplus.useClamp;
    cfg.surplus.clampCalibration = surplus["clampCalibration"] | cfg.surplus.clampCalibration;
    cfg.surplus.clampVoltage = surplus["clampVoltage"] | cfg.surplus.clampVoltage;
    cfg.surplus.attachedLoadWatts = surplus["attachedLoadWatts"] | cfg.surplus.attachedLoadWatts;
    cfg.surplus.useSolarAsMptt = surplus["useSolarAsMptt"] | cfg.surplus.useSolarAsMptt;
    cfg.surplus.useBmv = surplus["useBmv"] | cfg.surplus.useBmv;
    cfg.surplus.gridPhase = surplus["gridPhase"] | cfg.surplus.gridPhase;
    cfg.surplus.maxErrorTimeMs = surplus["maxErrorTimeMs"] | cfg.surplus.maxErrorTimeMs;
    cfg.surplus.pollIntervalMs = surplus["pollIntervalMs"] | cfg.surplus.pollIntervalMs;
    cfg.surplus.pwmSlaveOnPercent = surplus["pwmSlaveOnPercent"] | cfg.surplus.pwmSlaveOnPercent;
  }

  JsonObjectConst load = doc["load"];
  if (!load.isNull()) {
    cfg.load.pwmEnabled = load["pwmEnabled"] | cfg.load.pwmEnabled;
    cfg.load.manualMode = load["manualMode"] | cfg.load.manualMode;
    cfg.load.potTarget = load["potTarget"] | cfg.load.potTarget;
    cfg.load.manualControlPercent = load["manualControlPercent"] | cfg.load.manualControlPercent;
    cfg.load.autoControlPercent = load["autoControlPercent"] | cfg.load.autoControlPercent;
    cfg.load.pwmFrequencyHz = load["pwmFrequencyHz"] | cfg.load.pwmFrequencyHz;
    cfg.load.dimmerLowCost = load["dimmerLowCost"] | cfg.load.dimmerLowCost;
    cfg.load.maxPwmLowCost = load["maxPwmLowCost"] | cfg.load.maxPwmLowCost;
    cfg.load.maxWattsTariff = load["maxWattsTariff"] | cfg.load.maxWattsTariff;
    JsonObjectConst pid = load["pid"];
    if (!pid.isNull()) {
      cfg.load.pid.kp = pid["kp"] | cfg.load.pid.kp;
      cfg.load.pid.ki = pid["ki"] | cfg.load.pid.ki;
      cfg.load.pid.kd = pid["kd"] | cfg.load.pid.kd;
    }
    JsonArrayConst outputs = load["outputs"];
    if (!outputs.isNull()) {
      uint8_t i = 0;
      for (JsonObjectConst jo : outputs) {
        if (i >= 4) break;
        cfg.load.outputs[i].minPercent = jo["minPercent"] | cfg.load.outputs[i].minPercent;
        cfg.load.outputs[i].onWatts = jo["onWatts"] | cfg.load.outputs[i].onWatts;
        cfg.load.outputs[i].offWatts = jo["offWatts"] | cfg.load.outputs[i].offWatts;
        cfg.load.outputs[i].manual = jo["manual"] | cfg.load.outputs[i].manual;
        i++;
      }
    }
    JsonObjectConst offGrid = load["offGrid"];
    if (!offGrid.isNull()) {
      cfg.load.offGrid.enabled = offGrid["enabled"] | cfg.load.offGrid.enabled;
      cfg.load.offGrid.useVoltage = offGrid["useVoltage"] | cfg.load.offGrid.useVoltage;
      cfg.load.offGrid.socThreshold = offGrid["socThreshold"] | cfg.load.offGrid.socThreshold;
      cfg.load.offGrid.battWattsThreshold = offGrid["battWattsThreshold"] | cfg.load.offGrid.battWattsThreshold;
      cfg.load.offGrid.batteryVoltage = offGrid["batteryVoltage"] | cfg.load.offGrid.batteryVoltage;
      cfg.load.offGrid.voltageOffset = offGrid["voltageOffset"] | cfg.load.offGrid.voltageOffset;
    }
    cfg.load.potManPwmActive = load["potManPwmActive"] | cfg.load.potManPwmActive;
    cfg.load.potManPwmWatts = load["potManPwmWatts"] | cfg.load.potManPwmWatts;
    cfg.load.timerEnabled = load["timerEnabled"] | cfg.load.timerEnabled;
    cfg.load.timerStartHHMM = load["timerStartHHMM"] | cfg.load.timerStartHHMM;
    cfg.load.timerStopHHMM = load["timerStopHHMM"] | cfg.load.timerStopHHMM;
  }

  JsonObjectConst temp = doc["temperature"];
  if (!temp.isNull()) {
    cfg.temperature.enabled = temp["enabled"] | cfg.temperature.enabled;
    cfg.temperature.turnOnC = temp["turnOnC"] | cfg.temperature.turnOnC;
    cfg.temperature.turnOffC = temp["turnOffC"] | cfg.temperature.turnOffC;
    cfg.temperature.mode = temp["mode"] | cfg.temperature.mode;
    JsonArrayConst thermoAddr = temp["thermoSensorAddr"];
    if (!thermoAddr.isNull()) { uint8_t i = 0; for (JsonVariantConst b : thermoAddr) { if (i >= 8) break; cfg.temperature.thermoSensorAddr[i++] = b.as<uint8_t>(); } }
    JsonArrayConst triacAddr = temp["triacSensorAddr"];
    if (!triacAddr.isNull()) { uint8_t i = 0; for (JsonVariantConst b : triacAddr) { if (i >= 8) break; cfg.temperature.triacSensorAddr[i++] = b.as<uint8_t>(); } }
    JsonArrayConst customAddr = temp["customSensorAddr"];
    if (!customAddr.isNull()) { uint8_t i = 0; for (JsonVariantConst b : customAddr) { if (i >= 8) break; cfg.temperature.customSensorAddr[i++] = b.as<uint8_t>(); } }
    copyStr(temp["customSensorName"], cfg.temperature.customSensorName);
  }

  JsonObjectConst sys = doc["system"];
  if (!sys.isNull()) {
    copyStr(sys["language"], cfg.system.language);
    copyStr(sys["timezone"], cfg.system.timezone);
    copyStr(sys["ntpServer"], cfg.system.ntpServer);
    cfg.system.oledPower = sys["oledPower"] | cfg.system.oledPower;
    cfg.system.oledAutoOff = sys["oledAutoOff"] | cfg.system.oledAutoOff;
    cfg.system.flipScreen = sys["flipScreen"] | cfg.system.flipScreen;
    cfg.system.oledAutoOffMs = sys["oledAutoOffMs"] | cfg.system.oledAutoOffMs;
    cfg.system.oledBrightness = sys["oledBrightness"] | cfg.system.oledBrightness;
    cfg.system.alexaControl = sys["alexaControl"] | cfg.system.alexaControl;
    cfg.system.domoticzEnabled = sys["domoticzEnabled"] | cfg.system.domoticzEnabled;
    JsonArrayConst idx = sys["domoticzIdx"];
    if (!idx.isNull()) { uint8_t i = 0; for (JsonVariantConst v : idx) { if (i >= 3) break; cfg.system.domoticzIdx[i++] = v.as<uint16_t>(); } }
    cfg.system.weblogEnabled = sys["weblogEnabled"] | cfg.system.weblogEnabled;
    cfg.system.serialLogEnabled = sys["serialLogEnabled"] | cfg.system.serialLogEnabled;
    cfg.system.debugFlags = sys["debugFlags"] | cfg.system.debugFlags;
    copyStr(sys["adminPasswordB64"], cfg.system.adminPasswordB64);
  }

  JsonObjectConst energy = doc["energy"];
  if (!energy.isNull()) {
    cfg.energy.kwToday = energy["kwToday"] | cfg.energy.kwToday;
    cfg.energy.kwExportToday = energy["kwExportToday"] | cfg.energy.kwExportToday;
    cfg.energy.kwYesterday = energy["kwYesterday"] | cfg.energy.kwYesterday;
    cfg.energy.kwExportYesterday = energy["kwExportYesterday"] | cfg.energy.kwExportYesterday;
    cfg.energy.kwTotal = energy["kwTotal"] | cfg.energy.kwTotal;
    cfg.energy.kwExportTotal = energy["kwExportTotal"] | cfg.energy.kwExportTotal;
  }
}

} // namespace

bool ConfigStore::begin() {
  applyDefaults(g_config);

  if (!SPIFFS.exists(CONFIG_PATH)) {
    Logger::info("Config: /config.json não encontrado, a usar valores por omissão\n");
    save();
    return false;
  }

  File f = SPIFFS.open(CONFIG_PATH, FILE_READ);
  if (!f) {
    Logger::info("Config: erro ao abrir /config.json\n");
    return false;
  }

  DynamicJsonDocument doc(6144);
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Logger::info("Config: JSON inválido (%s), a usar valores por omissão\n", err.c_str());
    return false;
  }

  fromJson(doc, g_config);
  Logger::info("Config: configuração carregada (schema v%u)\n", g_config.schemaVersion);
  return true;
}

void ConfigStore::save() {
  DynamicJsonDocument doc(6144);
  toJson(g_config, doc);

  File f = SPIFFS.open(CONFIG_PATH, FILE_WRITE);
  if (!f) {
    Logger::info("Config: erro ao guardar /config.json\n");
    return;
  }
  serializeJson(doc, f);
  f.close();
}

void ConfigStore::resetToDefaults() {
  applyDefaults(g_config);
  save();
}

String ConfigStore::exportJson() {
  DynamicJsonDocument doc(6144);
  toJson(g_config, doc);
  String out;
  serializeJson(doc, out);
  return out;
}

bool ConfigStore::importJson(const String &json) {
  DynamicJsonDocument doc(6144);
  DeserializationError err = deserializeJson(doc, json);
  if (err) return false;
  fromJson(doc, g_config);
  save();
  return true;
}
