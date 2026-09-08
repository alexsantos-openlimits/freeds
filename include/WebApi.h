/*
  WebApi.h - Interface REST + SSE + OTA, e serviço dos ficheiros da SPA

  Substitui webserver_handlers.ino e webserver_processors.ino. Em vez de
  processadores de template HTML, expõe uma API JSON consumida pela nova
  SPA (ver data/index.html), o que também torna a interface mais fácil de
  automatizar/integrar (Home Assistant, scripts, etc.).
*/
#pragma once

#include <ESPAsyncWebServer.h>
#include "LoadController.h"
#include "TemperatureManager.h"
#include "NetworkManager.h"
#include "SurplusManager.h"
#include "CurrentClampSensor.h"

class WebApi {
public:
  // surplus: apontador para o apontador do gestor de excedentes ativo (a
  // instância pode ser substituída em runtime quando o utilizador muda de
  // modo); onSurplusModeChanged é chamado depois de gravar uma configuração
  // que altere surplus.mode, para o Lusol.ino recriar o gestor certo.
  void begin(LoadController *load, TemperatureManager *temperature, NetworkManager *network,
             SurplusManager **surplus, CurrentClampSensor *clamp, void (*onSurplusModeChanged)());

  void loop(); // envia o estado por SSE periodicamente

private:
  void registerApiRoutes();
  void registerSystemRoutes();
  String buildStatusJson();
  void handleConfigSection(AsyncWebServerRequest *request, uint8_t *data, size_t len, const char *section);
  void restartSoon();

  AsyncWebServer server_{80};
  AsyncEventSource events_{"/events"};

  LoadController *load_ = nullptr;
  TemperatureManager *temperature_ = nullptr;
  NetworkManager *network_ = nullptr;
  SurplusManager **surplus_ = nullptr;
  CurrentClampSensor *clamp_ = nullptr;
  void (*onSurplusModeChanged_)() = nullptr;

  unsigned long lastStatusPushMs_ = 0;
};
