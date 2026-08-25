#include "MqttPublisher.h"
#include "MqttService.h"
#include "AppConfig.h"
#include "Logger.h"

#include <WiFi.h>
#include <cstdlib>

namespace {
const char *kModeOptions[] = {"auto", "manual", "off"};
const char *kRelayOptions[] = {"auto", "on", "off"};

const char *relayModeStr(const RelayOutputState &s) {
  if (s.autoActivated) return "auto";
  return s.energized ? "on" : "off";
}
} // namespace

void MqttPublisher::begin(LoadController *load, SurplusManager **surplus, CurrentClampSensor *clamp,
                           TemperatureManager *temperature) {
  load_ = load;
  surplus_ = surplus;
  clamp_ = clamp;
  temperature_ = temperature;

  strlcpy(hostname_, ConfigStore::get().network.hostname, sizeof(hostname_));
  snprintf(statusTopic_, sizeof(statusTopic_), "%s/status", hostname_);
  snprintf(availabilityTopic_, sizeof(availabilityTopic_), "%s/availability", hostname_);

  for (uint8_t i = 0; i < 4; i++) {
    lastRelayOn_[i] = load_->relayState(i).energized;
    relayStatePublished_[i] = false;
  }
  discoveryPublishedOnce_ = false;
  lastCapabilitySignature_ = 0xFFFF;
}

void MqttPublisher::addCommonFields(JsonObject &doc, const char *objectId) {
  char uniqueId[48];
  snprintf(uniqueId, sizeof(uniqueId), "%s_%s", hostname_, objectId);
  // String(...) força a cópia do conteúdo para dentro do documento - sem
  // isto, o ArduinoJson pode só guardar o apontador para este buffer local,
  // que deixa de ser válido logo que esta função retornar (mas o
  // documento só é serializado depois, já em publishSensor/publishSelect/...).
  doc["unique_id"] = String(uniqueId);
  doc["availability_topic"] = availabilityTopic_;

  JsonObject dev = doc.createNestedObject("device");
  JsonArray ids = dev.createNestedArray("identifiers");
  ids.add(hostname_);
  dev["name"] = hostname_;
  dev["manufacturer"] = "FreeDS";
  dev["model"] = "Gestor de excedentes fotovoltaicos";
}

void MqttPublisher::publishSensor(const char *objectId, const char *name, const char *unit, const char *deviceClass,
                                   const char *stateClass, const char *valueTemplate) {
  char topic[80];
  snprintf(topic, sizeof(topic), "homeassistant/sensor/%s/%s/config", hostname_, objectId);

  DynamicJsonDocument doc(768);
  JsonObject root = doc.to<JsonObject>();
  root["name"] = name;
  root["state_topic"] = statusTopic_;
  root["value_template"] = valueTemplate;
  if (unit) root["unit_of_measurement"] = unit;
  if (deviceClass) root["device_class"] = deviceClass;
  if (stateClass) root["state_class"] = stateClass;
  addCommonFields(root, objectId);

  String out;
  serializeJson(doc, out);
  MqttService::publish(topic, out.c_str(), true);
}

void MqttPublisher::publishBinarySensor(const char *objectId, const char *name, const char *deviceClass,
                                         const char *valueTemplate) {
  char topic[80];
  snprintf(topic, sizeof(topic), "homeassistant/binary_sensor/%s/%s/config", hostname_, objectId);

  DynamicJsonDocument doc(768);
  JsonObject root = doc.to<JsonObject>();
  root["name"] = name;
  root["state_topic"] = statusTopic_;
  root["value_template"] = valueTemplate;
  if (deviceClass) root["device_class"] = deviceClass;
  addCommonFields(root, objectId);

  String out;
  serializeJson(doc, out);
  MqttService::publish(topic, out.c_str(), true);
}

void MqttPublisher::publishSelect(const char *objectId, const char *name, const char *valueTemplate,
                                   const char *commandTopic, const char *const *options, uint8_t optionCount) {
  char topic[80];
  snprintf(topic, sizeof(topic), "homeassistant/select/%s/%s/config", hostname_, objectId);

  DynamicJsonDocument doc(768);
  JsonObject root = doc.to<JsonObject>();
  root["name"] = name;
  root["state_topic"] = statusTopic_;
  root["value_template"] = valueTemplate;
  root["command_topic"] = commandTopic;
  JsonArray opts = root.createNestedArray("options");
  for (uint8_t i = 0; i < optionCount; i++) opts.add(options[i]);
  addCommonFields(root, objectId);

  String out;
  serializeJson(doc, out);
  MqttService::publish(topic, out.c_str(), true);
}

void MqttPublisher::publishNumber(const char *objectId, const char *name, const char *commandTopic, float min,
                                   float max, float step) {
  char topic[80];
  snprintf(topic, sizeof(topic), "homeassistant/number/%s/%s/config", hostname_, objectId);

  DynamicJsonDocument doc(768);
  JsonObject root = doc.to<JsonObject>();
  root["name"] = name;
  root["command_topic"] = commandTopic;
  root["min"] = min;
  root["max"] = max;
  root["step"] = step;
  root["unit_of_measurement"] = "%";
  addCommonFields(root, objectId);

  String out;
  serializeJson(doc, out);
  MqttService::publish(topic, out.c_str(), true);
}

void MqttPublisher::removeDiscovery(const char *component, const char *objectId) {
  char topic[80];
  snprintf(topic, sizeof(topic), "homeassistant/%s/%s/%s/config", component, hostname_, objectId);
  MqttService::publish(topic, "", true);
}

void MqttPublisher::publishDiscoveryIfNeeded() {
  const SurplusManager *sm = (surplus_ && *surplus_) ? *surplus_ : nullptr;
  const PowerReadingFields &f = sm ? sm->fields() : PowerReadingFields{};
  bool tempEnabled = ConfigStore::get().temperature.enabled;

  uint16_t sig = 0;
  if (f.batteryWatts || f.batterySoc) sig |= 0x0001;
  if (f.pv1) sig |= 0x0002;
  if (f.pv2) sig |= 0x0004;
  if (f.inverterTemperature) sig |= 0x0008;
  if (f.gridVoltage) sig |= 0x0010;
  if (f.gridCurrent) sig |= 0x0020;
  if (f.frequency) sig |= 0x0040;
  if (f.powerFactor) sig |= 0x0080;
  if (tempEnabled) sig |= 0x0100;

  if (discoveryPublishedOnce_ && sig == lastCapabilitySignature_) return;
  bool firstTime = !discoveryPublishedOnce_;
  discoveryPublishedOnce_ = true;
  lastCapabilitySignature_ = sig;

  Logger::info("MQTT: a publicar descoberta Home Assistant\n");

  if (firstTime) {
    char cmdMode[48], cmdPwm[48];
    snprintf(cmdMode, sizeof(cmdMode), "%s/cmd/mode", hostname_);
    snprintf(cmdPwm, sizeof(cmdPwm), "%s/cmd/manual_pwm", hostname_);

    publishSelect("working_mode", "Modo de funcionamento", "{{ value_json.workingMode }}", cmdMode, kModeOptions, 3);
    publishNumber("manual_pwm", "PWM manual", cmdPwm, 0, 100, 1);
    publishSensor("pwm_percent", "PWM", "%", nullptr, "measurement", "{{ value_json.pwmPercent }}");
    publishSensor("grid_watts", "Potencia de rede", "W", "power", "measurement", "{{ value_json.gridWatts }}");
    publishSensor("solar_watts", "Potencia solar", "W", "power", "measurement", "{{ value_json.solarWatts }}");
    publishSensor("solar_today", "Producao solar hoje", "kWh", "energy", "total_increasing",
                   "{{ value_json.solarWattsToday }}");
    publishSensor("load_watts", "Potencia da carga", "W", "power", "measurement", "{{ value_json.loadWatts }}");
    publishSensor("energy_today_imported", "Energia importada hoje", "kWh", "energy", "total_increasing",
                   "{{ value_json.energyTodayImported }}");
    publishSensor("energy_today_exported", "Energia exportada hoje", "kWh", "energy", "total_increasing",
                   "{{ value_json.energyTodayExported }}");
    publishSensor("energy_total_imported", "Energia importada total", "kWh", "energy", "total_increasing",
                   "{{ value_json.energyTotalImported }}");
    publishSensor("energy_total_exported", "Energia exportada total", "kWh", "energy", "total_increasing",
                   "{{ value_json.energyTotalExported }}");
    publishSensor("wifi_rssi", "Sinal Wi-Fi", "dBm", "signal_strength", "measurement", "{{ value_json.wifiRssi }}");
    publishBinarySensor("source_connected", "Fonte de dados ligada", "connectivity",
                         "{{ 'ON' if value_json.sourceConnected else 'OFF' }}");

    for (uint8_t i = 0; i < 4; i++) {
      char objectId[16], name[16], cmdTopic[48], valueTemplate[48];
      snprintf(objectId, sizeof(objectId), "relay_%u", (unsigned)(i + 1));
      snprintf(name, sizeof(name), "Saida %u", (unsigned)(i + 1));
      snprintf(cmdTopic, sizeof(cmdTopic), "%s/cmd/relay%u", hostname_, (unsigned)i);
      snprintf(valueTemplate, sizeof(valueTemplate), "{{ value_json.relay%uMode }}", (unsigned)i);
      publishSelect(objectId, name, valueTemplate, cmdTopic, kRelayOptions, 3);
    }
  }

  // Entidades condicionadas às capacidades da fonte de dados atual - só
  // aparecem no Home Assistant quando fazem sentido; removidas (payload
  // vazio) quando a fonte deixa de as reportar.
  if (sig & 0x0001) {
    publishSensor("battery_watts", "Potencia da bateria", "W", "power", "measurement", "{{ value_json.batteryWatts }}");
    publishSensor("battery_soc", "Estado de carga da bateria", "%", "battery", "measurement",
                   "{{ value_json.batterySoc }}");
  } else {
    removeDiscovery("sensor", "battery_watts");
    removeDiscovery("sensor", "battery_soc");
  }

  if (sig & 0x0002) {
    publishSensor("pv1_watts", "Potencia String 1", "W", "power", "measurement", "{{ value_json.pv1Watts }}");
  } else {
    removeDiscovery("sensor", "pv1_watts");
  }

  if (sig & 0x0004) {
    publishSensor("pv2_watts", "Potencia String 2", "W", "power", "measurement", "{{ value_json.pv2Watts }}");
  } else {
    removeDiscovery("sensor", "pv2_watts");
  }

  if (sig & 0x0008) {
    publishSensor("inverter_temperature", "Temperatura do inversor", "°C", "temperature", "measurement",
                   "{{ value_json.inverterTemperature }}");
  } else {
    removeDiscovery("sensor", "inverter_temperature");
  }

  if (sig & 0x0010) {
    publishSensor("grid_voltage", "Tensao de rede", "V", "voltage", "measurement", "{{ value_json.gridVoltage }}");
  } else {
    removeDiscovery("sensor", "grid_voltage");
  }

  if (sig & 0x0020) {
    publishSensor("grid_current", "Corrente de rede", "A", "current", "measurement", "{{ value_json.gridCurrent }}");
  } else {
    removeDiscovery("sensor", "grid_current");
  }

  if (sig & 0x0040) {
    publishSensor("grid_frequency", "Frequencia de rede", "Hz", "frequency", "measurement",
                   "{{ value_json.frequency }}");
  } else {
    removeDiscovery("sensor", "grid_frequency");
  }

  if (sig & 0x0080) {
    publishSensor("power_factor", "Fator de potencia", nullptr, "power_factor", "measurement",
                   "{{ value_json.powerFactor }}");
  } else {
    removeDiscovery("sensor", "power_factor");
  }

  if (sig & 0x0100) {
    publishSensor("temp_thermo", "Temperatura do termoacumulador", "°C", "temperature", "measurement",
                   "{{ value_json.thermoTemperature }}");
    publishSensor("temp_triac", "Temperatura do triac", "°C", "temperature", "measurement",
                   "{{ value_json.triacTemperature }}");
  } else {
    removeDiscovery("sensor", "temp_thermo");
    removeDiscovery("sensor", "temp_triac");
  }
}

void MqttPublisher::subscribeCommands() {
  if (commandsSubscribed_) return;
  commandsSubscribed_ = true;

  char cmdMode[48], cmdPwm[48];
  snprintf(cmdMode, sizeof(cmdMode), "%s/cmd/mode", hostname_);
  snprintf(cmdPwm, sizeof(cmdPwm), "%s/cmd/manual_pwm", hostname_);

  MqttService::onTopic(cmdMode, [this](const char *t, const char *p) { handleCommand(t, p); });
  MqttService::onTopic(cmdPwm, [this](const char *t, const char *p) { handleCommand(t, p); });

  static char cmdRelayTopics[4][48];
  for (uint8_t i = 0; i < 4; i++) {
    snprintf(cmdRelayTopics[i], sizeof(cmdRelayTopics[i]), "%s/cmd/relay%u", hostname_, (unsigned)i);
    MqttService::onTopic(cmdRelayTopics[i], [this](const char *t, const char *p) { handleCommand(t, p); });
  }
}

void MqttPublisher::handleCommand(const char *topic, const char *payload) {
  String t(topic);

  if (t.endsWith("/cmd/mode")) {
    String mode(payload);
    if (mode == "auto") load_->setWorkingMode(LoadWorkingMode::Auto);
    else if (mode == "manual") load_->setWorkingMode(LoadWorkingMode::Manual);
    else if (mode == "off") load_->setWorkingMode(LoadWorkingMode::Off);
    return;
  }

  if (t.endsWith("/cmd/manual_pwm")) {
    ConfigStore::get().load.manualControlPercent = (uint16_t)constrain(atoi(payload), 0, 100);
    ConfigStore::save();
    return;
  }

  for (uint8_t i = 0; i < 4; i++) {
    char suffix[16];
    snprintf(suffix, sizeof(suffix), "/cmd/relay%u", (unsigned)i);
    if (t.endsWith(suffix)) {
      String mode(payload);
      if (mode == "on") load_->setRelayManual(i, true);
      else if (mode == "off") load_->setRelayManual(i, false);
      else load_->setRelayAuto(i);
      return;
    }
  }
}

void MqttPublisher::publishStatusJson() {
  const SurplusManager *sm = (surplus_ && *surplus_) ? *surplus_ : nullptr;
  static const PowerReading emptyReading;
  const PowerReading &r = sm ? sm->reading() : emptyReading;
  const EnergyTotals &e = ConfigStore::get().energy;

  DynamicJsonDocument doc(1024);
  doc["workingMode"] = load_->workingMode() == LoadWorkingMode::Manual ? "manual"
                       : load_->workingMode() == LoadWorkingMode::Off  ? "off"
                                                                        : "auto";
  doc["pwmPercent"] = load_->pwmPercent();
  doc["gridWatts"] = r.gridWatts;
  doc["gridVoltage"] = r.gridVoltage;
  doc["gridCurrent"] = r.gridCurrent;
  doc["frequency"] = r.frequency;
  doc["powerFactor"] = r.powerFactor;
  doc["solarWatts"] = r.solarWatts;
  doc["solarWattsToday"] = r.solarWattsToday;
  doc["batteryWatts"] = r.batteryWatts;
  doc["batterySoc"] = r.batterySoc;
  doc["loadWatts"] = r.loadWatts != 0 ? r.loadWatts : (clamp_ ? clamp_->lastWatts() : 0);
  doc["pv1Watts"] = r.pv1Watts;
  doc["pv2Watts"] = r.pv2Watts;
  doc["inverterTemperature"] = r.inverterTemperature;
  doc["thermoTemperature"] = temperature_ ? temperature_->thermoTemperature() : -127;
  doc["triacTemperature"] = temperature_ ? temperature_->triacTemperature() : -127;
  doc["energyTodayImported"] = e.kwToday;
  doc["energyTodayExported"] = e.kwExportToday;
  doc["energyTotalImported"] = e.kwTotal;
  doc["energyTotalExported"] = e.kwExportTotal;
  doc["sourceConnected"] = sm ? sm->isConnected() : false;
  doc["wifiRssi"] = WiFi.RSSI();
  for (uint8_t i = 0; i < 4; i++) {
    char key[16];
    snprintf(key, sizeof(key), "relay%uMode", (unsigned)i);
    // String(key) força a cópia da chave - o mesmo buffer "key" é reutilizado
    // a cada iteração, por isso guardar só o apontador faria as 4 chaves
    // acabarem todas iguais (com o conteúdo da última iteração).
    doc[String(key)] = relayModeStr(load_->relayState(i));
  }

  String out;
  serializeJson(doc, out);
  MqttService::publish(statusTopic_, out.c_str());
}

void MqttPublisher::loop() {
  if (!MqttService::connected()) {
    wasConnected_ = false;
    return;
  }

  if (!wasConnected_) {
    wasConnected_ = true;
    // Reafirma "online" (retido) a cada nova ligação - o LWT só publica
    // "offline" por nós quando a ligação cai sem aviso; sem isto, depois
    // de uma queda e reconexão o tópico de disponibilidade ficava parado
    // em "offline" para o Home Assistant.
    MqttService::publish(availabilityTopic_, "online", true);
  }

  subscribeCommands();
  publishDiscoveryIfNeeded();

  // Estado dos relés: publica de imediato sempre que mudam (tal como o
  // antigo publisher(config.R0x_mqtt, "ON"/"OFF") em pwm.ino), e volta a
  // publicar pelo menos uma vez após reconectar ao broker.
  const MqttConfig &mqtt = ConfigStore::get().mqtt;
  for (uint8_t i = 0; i < 4; i++) {
    bool on = load_->relayState(i).energized;
    if (on != lastRelayOn_[i] || !relayStatePublished_[i]) {
      lastRelayOn_[i] = on;
      relayStatePublished_[i] = true;
      if (strlen(mqtt.relayTopic[i]) > 0) {
        MqttService::publish(mqtt.relayTopic[i], on ? "ON" : "OFF", true);
      }
    }
  }

  uint32_t interval = mqtt.publishIntervalMs > 0 ? mqtt.publishIntervalMs : 5000;
  if (millis() - lastStatusPublishMs_ >= interval) {
    lastStatusPublishMs_ = millis();
    publishStatusJson();
  }
}
