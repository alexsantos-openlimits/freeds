/*
  HttpInverterManager.h - Gestor de excedentes para inversores/contadores
  falados por série (Solax V2 via módulo ESP-01) ou por HTTP direto (Solax
  V2 local/V1, Wibeee, Shelly EM, Fronius API e o modo "escravo" que lê os
  dados de outro Lusol "mestre").

  Substitui:
    - inverter.ino          (readESP01()/parseJson() - Solax V2 por série)
    - asyncHttpClient.ino   (runAsyncClient()/processingData() - cliente TCP
                              assíncrono cru e parseJsonv1/v2local/v3local/
                              Fronius)
    - wibeee.ino            (parseWibeee())
    - shelly.ino            (parseShellyEM())
    - master_lusol.ino     (parseMasterFreeDs())

  O modo é escolhido em runtime a partir de ConfigStore::get().surplus.mode;
  esta classe cobre exatamente os modos HTTP_API que não usam Modbus/MQTT/UDP:

    SOLAX_V2       (21) - série, UART2 <-> módulo ESP-01 (RX=17, TX=5, fixo
                          pelo hardware). NÃO usa rede/HTTP.
    SOLAX_V2_LOCAL (22) - HTTP POST /?optType=ReadRealTimeData&pwd=admin
                          (JSON "Data": [...], formato v2 ou v3 conforme
                          surplus.solaxVersion)
    SOLAX_V1       (23) - HTTP GET /api/realTimeData.htm (JSON "Data": [...])
    WIBEEE         (24) - HTTP GET /en/status.xml (XML/texto)
    SHELLY_EM      (25) - HTTP GET /emeter/0 e /emeter/1 alternados (JSON)
    FRONIUS_API    (26) - HTTP GET /solar_api/v1/GetPowerFlowRealtimeData.fcgi
    SLAVE_MODE     (27) - HTTP GET /masterdata (JSON de outro Lusol mestre)

  Os pedidos HTTP usam um AsyncClient (AsyncTCP) em modo "cru" tal como o
  firmware original (sem HTTPClient bloqueante): o pedido é escrito à mão
  como texto HTTP/1.1 e a resposta é acumulada em buffer nos callbacks
  onData()/onDisconnect(), exatamente como em asyncHttpClient.ino.
*/
#pragma once

#include <Arduino.h>
#include <AsyncTCP.h>

#include "SurplusManager.h"

class HttpInverterManager : public SurplusManagerBase {
public:
  HttpInverterManager();
  ~HttpInverterManager() override;

  void begin() override;
  void loop() override;
  const char *name() const override { return "HTTP/Serie Inversor"; }

  // Só tem significado em SLAVE_MODE: true quando a última mensagem do
  // Lusol mestre reportou uma percentagem de PWM (ou paragem por
  // temperatura) abaixo do limiar configurado em surplus.pwmSlaveOnPercent -
  // equivalente ao antigo "slave.masterPwmValue >= config.pwmSlaveOn" (aqui
  // invertido: true significa "o mestre pede para desligar o PWM deste
  // escravo"). Quem integra este gestor no controlador principal deve ler
  // este método para decidir Flags.pwmIsWorking / shutdownPwm().
  bool masterRequestsPwmDisable() const { return masterRequestsPwmDisable_; }

private:
  // ---------------- Solax V2 por série (UART2 <-> ESP-01) ----------------
  void pollEsp01Serial();
  void parseEsp01Json(const String &json);

  // ---------------- Cliente TCP assíncrono cru (HTTP) ----------------
  void startHttpRequest();
  void closeHttpClient(bool abortConnection);
  void resetMessageBuffer();
  void buildRequestLine(char *out, size_t outSize) const;
  void onClientConnect(AsyncClient *client);
  void onClientData(AsyncClient *client, const char *data, size_t len);
  void onClientDisconnect();
  void processHttpResponse();

  // ---------------- Parsers (um por modo/fonte, campos originais) --------
  void parseSolaxV2Local(const char *json); // Data[] formato "v2"
  void parseSolaxV3Local(const char *json); // Data[] formato "v3"
  void parseSolaxV1(const char *json);
  void parseWibeeeXml(const char *xml);
  void parseShellyEm(const char *json, uint8_t sensor);
  void parseFronius(const char *json);
  void parseMasterFreeDs(const char *json);

  // Utilitário local equivalente ao "midString()" de Support_functions.ino
  // (extrai o texto entre dois marcadores). Não depende de nenhum global.
  static String midString(const String &src, const char *startTag, const char *endTag);

  // Réplica do defineWebMonitorFields() original: para um dado "wversion"
  // reportado por um Lusol mestre em SLAVE_MODE, indica que campos vêm
  // realmente preenchidos na mensagem "/masterdata". Necessário para não
  // confundir um campo ausente (0 por omissão no JSON) com uma leitura
  // válida de 0 W.
  static void masterFieldMask(uint8_t masterWversion, PowerReadingFields &out);

  HardwareSerial espSerial_;

  AsyncClient *client_ = nullptr;
  uint32_t connectionStartMs_ = 0;
  uint32_t receivingDataStartMs_ = 0;
  uint32_t lastRequestMs_ = 0;
  bool receivingData_ = false;
  bool firstChunk_ = false;
  bool responseReady_ = false; // equivalente à antiga flag global "processData"
  uint8_t shellySensor_ = 1;   // alterna 1/2 tal como o original

  static const uint16_t kMaxMessageSize = 4999;
  char messageBuffer_[kMaxMessageSize + 1] = {0};
  uint16_t messageLength_ = 0;
  uint16_t totalMessageLength_ = 0;
  uint16_t payloadStart_ = 0;

  // SLAVE_MODE: último valor de PWM reportado pelo mestre (0-100, ou 100 se
  // "tempShutdown" estiver ativo) e resultado já calculado de
  // masterRequestsPwmDisable().
  int masterPwmValue_ = 100;
  bool masterRequestsPwmDisable_ = false;
};
