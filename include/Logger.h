/*
  Logger.h - Registo de eventos (substitui a antiga macro INFOV global)
*/
#pragma once

#include <Arduino.h>
#include <functional>

class Logger {
public:
  using Sink = std::function<void(const char *)>;

  // Regista quem recebe as mensagens para reenviar por SSE ("/weblog").
  static void setWeblogSink(Sink sink);

  // Escreve na Serial (se ativo) e envia ao sink de weblog (se ativo).
  // Devolve o número de caracteres escritos, tal como printf.
  static int info(const char *format, ...);

  // Bit 0x01 de system.debugFlags - ativa mensagens de diagnóstico extra
  // (ligação Wi-Fi/MQTT, mensagens recebidas, etc.) sem poluir o registo
  // normal. Centralizado aqui para não haver várias cópias divergentes.
  static bool debugEnabled();
};
