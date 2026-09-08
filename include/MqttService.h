/*
  MqttService.h - Cliente MQTT partilhado (ligação, publicação e distribuição
  de mensagens recebidas por tópico)

  Substitui a mistura de responsabilidades que existia em mqtt.ino (ligação,
  publicação do estado dos relés E leitura de dados de excedente, tudo no
  mesmo ficheiro). Agora este serviço trata apenas da ligação/mensagens; os
  gestores de excedentes que precisem de dados por MQTT (ex.: MqttSurplusManager)
  registam-se com onTopic().
*/
#pragma once

#include <Arduino.h>
#include <functional>

class MqttService {
public:
  static void begin();
  static void loop(); // trata da reconexão periódica

  // A chamar depois de qualquer alteração à configuração MQTT (secção
  // "mqtt" gravada pela interface web): desliga se estava ligado e volta a
  // configurar/ligar o cliente com os valores atuais - sem isto, mudanças
  // feitas na interface só tinham efeito depois de reiniciar o dispositivo.
  static void reload();

  static bool connected();
  static bool enabled();
  static void publish(const char *topic, const char *payload, bool retain = false);

  using MessageHandler = std::function<void(const char *topic, const char *payload)>;

  // Subscreve `topic` (se ainda não ligado, fica em fila e subscreve ao ligar)
  // e chama handler(topic, payload) sempre que chegar uma mensagem completa.
  static void onTopic(const char *topic, MessageHandler handler);

  // Remove todos os handlers registados com onTopic(). A chamar antes de
  // destruir um objeto que tenha registado handlers (ex.: ao recriar o
  // gestor de excedentes) - sem isto, um handler com "this" de um objeto já
  // destruído ficava na lista e seria chamado na próxima mensagem
  // correspondente (use-after-free).
  static void clearTopics();
};
