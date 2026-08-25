#include "MqttPublisher.h"
#include "MqttService.h"
#include "AppConfig.h"

#include <ArduinoJson.h>

void MqttPublisher::begin(LoadController *load, SurplusManager **surplus, CurrentClampSensor *clamp) {
  load_ = load;
  surplus_ = surplus;
  clamp_ = clamp;
  snprintf(statusTopic_, sizeof(statusTopic_), "%s/status", ConfigStore::get().network.hostname);
  for (uint8_t i = 0; i < 4; i++) {
    lastRelayOn_[i] = load_->relayState(i).energized;
    relayStatePublished_[i] = false;
  }
}

void MqttPublisher::publishStatusJson() {
  const SurplusManager *sm = (surplus_ && *surplus_) ? *surplus_ : nullptr;
  static const PowerReading emptyReading;
  const PowerReading &r = sm ? sm->reading() : emptyReading;

  DynamicJsonDocument doc(768);
  doc["workingMode"] = load_->workingMode() == LoadWorkingMode::Manual ? "manual"
                       : load_->workingMode() == LoadWorkingMode::Off  ? "off"
                                                                        : "auto";
  doc["pwmPercent"] = load_->pwmPercent();
  doc["gridWatts"] = r.gridWatts;
  doc["solarWatts"] = r.solarWatts;
  doc["batteryWatts"] = r.batteryWatts;
  doc["batterySoc"] = r.batterySoc;
  doc["loadWatts"] = r.loadWatts != 0 ? r.loadWatts : (clamp_ ? clamp_->lastWatts() : 0);
  doc["sourceConnected"] = sm ? sm->isConnected() : false;

  String out;
  serializeJson(doc, out);
  MqttService::publish(statusTopic_, out.c_str());
}

void MqttPublisher::loop() {
  if (!MqttService::connected()) return;

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
