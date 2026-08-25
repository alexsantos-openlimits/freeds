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
bool g_callbacksRegistered = false;

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

bool debugEnabled() { return (ConfigStore::get().system.debugFlags & 0x01) != 0; }

const char *disconnectReasonText(AsyncMqttClientDisconnectReason reason) {
  switch (reason) {
    case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED: return "TCP desligado (verifica IP/porta do broker e a rede)";
    case AsyncMqttClientDisconnectReason::MQTT_UNACCEPTABLE_PROTOCOL_VERSION: return "versao de protocolo MQTT rejeitada pelo broker";
    case AsyncMqttClientDisconnectReason::MQTT_IDENTIFIER_REJECTED: return "client id rejeitado pelo broker";
    case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE: return "servidor indisponivel";
    case AsyncMqttClientDisconnectReason::MQTT_MALFORMED_CREDENTIALS: return "utilizador/password mal formados";
    case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED: return "utilizador/password rejeitados pelo broker (nao autorizado)";
    case AsyncMqttClientDisconnectReason::ESP8266_NOT_ENOUGH_SPACE: return "memoria insuficiente no ESP32";
    case AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT: return "certificado TLS invalido";
    default: return "motivo desconhecido";
  }
}

void onMqttConnect(bool sessionPresent) {
  Logger::info("MQTT: ligado ao broker\n");
  subscribeAll();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  if (debugEnabled()) {
    Logger::info("MQTT: desligado do broker - %s\n", disconnectReasonText(reason));
  } else {
    Logger::info("MQTT: desligado do broker\n");
  }
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

// AsyncMqttClient::onConnect/onDisconnect/onMessage acumulam num vector a
// cada chamada (não substituem o anterior) - por isso os callbacks só podem
// ser registados uma única vez, independentemente de quantas vezes begin()
// ou reload() sejam chamados.
void ensureCallbacksRegistered() {
  if (g_callbacksRegistered) return;
  g_callbacksRegistered = true;
  g_client.onConnect(onMqttConnect);
  g_client.onDisconnect(onMqttDisconnect);
  g_client.onMessage(onMqttMessage);
}

void configureAndConnect() {
  const MqttConfig &cfg = ConfigStore::get().mqtt;
  ensureCallbacksRegistered();

  if (g_client.connected()) g_client.disconnect(true);

  if (!cfg.enabled) {
    g_configured = false;
    return;
  }

  g_client.setClientId(ConfigStore::get().network.hostname);
  g_client.setKeepAlive(30);
  g_client.setCredentials(cfg.user, cfg.password);
  g_client.setServer(cfg.broker, cfg.port);
  g_configured = true;
  g_lastReconnectAttempt = millis();

  if (debugEnabled()) {
    Logger::info("MQTT: a ligar a %s:%u como \"%s\" (utilizador: %s)\n", cfg.broker, (unsigned)cfg.port,
                 ConfigStore::get().network.hostname, strlen(cfg.user) ? cfg.user : "(sem utilizador)");
  }
  g_client.connect();
}

} // namespace

void MqttService::begin() { configureAndConnect(); }

void MqttService::reload() {
  Logger::info("MQTT: a aplicar nova configuracao\n");
  configureAndConnect();
}

void MqttService::loop() {
  if (!g_configured) return;
  if (!g_client.connected() && millis() - g_lastReconnectAttempt > 5000) {
    g_lastReconnectAttempt = millis();
    if (debugEnabled()) Logger::info("MQTT: a tentar religar...\n");
    g_client.connect();
  }
}

bool MqttService::connected() { return g_configured && g_client.connected(); }
bool MqttService::enabled() { return g_configured; }

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

void MqttService::clearTopics() { g_handlers.clear(); }
