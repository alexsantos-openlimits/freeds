/*
  MqttPublisher.h - Publicação periódica de estado no MQTT + descoberta
  automática para o Home Assistant (MQTT Discovery)

  Substitui a parte de publish() que existia em mqtt.ino (estado dos relés
  por tópico individual + republicação periódica), e acrescenta uma
  funcionalidade nova: publica a configuração de descoberta MQTT do Home
  Assistant (tópicos "homeassistant/.../config") para que os sensores,
  o modo de funcionamento e os relés apareçam automaticamente como
  entidades de um dispositivo no HA, sem precisar de nenhuma integração
  HACS dedicada. Também subscreve tópicos de comando para que o HA possa
  controlar o modo de funcionamento, os relés e o PWM manual.
*/
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "LoadController.h"
#include "SurplusManager.h"
#include "CurrentClampSensor.h"
#include "TemperatureManager.h"

class MqttPublisher {
public:
  void begin(LoadController *load, SurplusManager **surplus, CurrentClampSensor *clamp,
             TemperatureManager *temperature);
  void loop();

  // MqttService::clearTopics() (chamado sempre que o gestor de excedentes é
  // recriado - ver Lusol.ino::recreateSurplusManager()) esvazia TODAS as
  // subscrições MQTT, incluindo os tópicos de comando registados aqui. Sem
  // isto, os comandos do Home Assistant (modo, relés, PWM manual) ficavam
  // mortos para sempre depois da primeira alteração na página de Gestor de
  // Excedentes ou de MQTT, até reiniciar o dispositivo.
  void resubscribeCommands();

private:
  void publishStatusJson();
  void publishDiscoveryIfNeeded();
  void publishSensor(const char *objectId, const char *name, const char *unit, const char *deviceClass,
                      const char *stateClass, const char *valueTemplate);
  void publishBinarySensor(const char *objectId, const char *name, const char *deviceClass, const char *valueTemplate);
  void publishSelect(const char *objectId, const char *name, const char *valueTemplate, const char *commandTopic,
                      const char *const *options, uint8_t optionCount);
  void publishNumber(const char *objectId, const char *name, const char *commandTopic, float min, float max, float step);
  void removeDiscovery(const char *component, const char *objectId);
  void addCommonFields(JsonObject &doc, const char *objectId);
  void subscribeCommands();
  void handleCommand(const char *topic, const char *payload);

  LoadController *load_ = nullptr;
  SurplusManager **surplus_ = nullptr;
  CurrentClampSensor *clamp_ = nullptr;
  TemperatureManager *temperature_ = nullptr;

  bool lastRelayOn_[4] = {false, false, false, false};
  bool relayStatePublished_[4] = {false, false, false, false};
  unsigned long lastStatusPublishMs_ = 0;
  char hostname_[24] = {0};
  char statusTopic_[40] = {0};
  char availabilityTopic_[40] = {0};

  // "Assinatura" das capacidades atualmente conhecidas (que campos a fonte
  // de dados reporta + se a temperatura está ativa) - quando muda,
  // republica a descoberta para o Home Assistant refletir a nova
  // disponibilidade de sensores (ex.: mudar de gestor de excedentes).
  uint16_t lastCapabilitySignature_ = 0xFFFF;
  bool discoveryPublishedOnce_ = false;
  bool commandsSubscribed_ = false;
  bool wasConnected_ = false;
};
