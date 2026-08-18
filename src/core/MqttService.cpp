#include "MqttService.h"
#include "AppConfig.h"
#include "Logger.h"

#include <AsyncMqttClient.h>
#include <vector>

namespace {

struct TopicHandler {
  String topic;
  MqttService::MessageHandler handler;
};

AsyncMqttClient g_client;
std::vector<TopicHandler> g_handlers;
unsigned long g_lastReconnectAttempt = 0;
bool g_configured = false;

// Compara um tópico recebido com um filtro de subscrição simples (suporta
// '#' no final, à semelhança do MQTT normal). Suficiente para os tópicos
// fixos usados pelos gestores de excedentes.
bool topicMatches(const String &filter, const char *topic) {
  if (filter.endsWith("#")) {
    String prefix = filter.substring(0, filter.length() - 1);
    return strncmp(prefix.c_str(), topic, prefix.length()) == 0;
  }
  return filter.equals(topic);
}

void subscribeAll() {
  for (auto &h : g_handlers) {
    g_client.subscribe(h.topic.c_str(), 0);
  }
}

void onMqttConnect(bool sessionPresent) {
  Logger::info("MQTT: ligado ao broker\n");
  subscribeAll();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Logger::info("MQTT: desligado do broker (motivo %d)\n", (int)reason);
}

void onMqttMessage(char *topic, char *payload, AsyncMqttClientMessageProperties properties,
                    size_t len, size_t index, size_t total) {
  // Mensagens fragmentadas (payload > ~1460 bytes) não são suportadas pelos
  // gestores de excedentes atuais (usam sempre JSON pequeno); ignoramos
  // fragmentos fora do caso simples de mensagem única e completa.
  if (index != 0 || total != len) return;

  static char buffer[2048];
  size_t copyLen = min(len, sizeof(buffer) - 1);
  memcpy(buffer, payload, copyLen);
  buffer[copyLen] = '\0';

  for (auto &h : g_handlers) {
    if (topicMatches(h.topic, topic)) {
      h.handler(topic, buffer);
    }
  }
}

} // namespace

void MqttService::begin() {
  const MqttConfig &cfg = ConfigStore::get().mqtt;
  if (!cfg.enabled) return;

  g_client.onConnect(onMqttConnect);
  g_client.onDisconnect(onMqttDisconnect);
  g_client.onMessage(onMqttMessage);
  g_client.setClientId(ConfigStore::get().network.hostname);
  g_client.setKeepAlive(30);
  g_client.setCredentials(cfg.user, cfg.password);
  g_client.setServer(cfg.broker, cfg.port);
  g_configured = true;
  g_client.connect();
}

void MqttService::loop() {
  if (!g_configured) return;
  if (!g_client.connected() && millis() - g_lastReconnectAttempt > 5000) {
    g_lastReconnectAttempt = millis();
    g_client.connect();
  }
}

bool MqttService::connected() { return g_configured && g_client.connected(); }

void MqttService::publish(const char *topic, const char *payload, bool retain) {
  if (!connected()) return;
  g_client.publish(topic, 0, retain, payload);
}

void MqttService::onTopic(const char *topic, MessageHandler handler) {
  g_handlers.push_back({String(topic), handler});
  if (connected()) {
    g_client.subscribe(topic, 0);
  }
}
