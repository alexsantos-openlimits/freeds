/*
  Logger.h - Registo de eventos (substitui a antiga macro INFOV global)

  Copyright (C) 2020-2026 Pablo Zerón (https://github.com/pablozg/freeds)

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.
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
};
