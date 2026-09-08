/*
  HttpInverterManager.cpp - ver HttpInverterManager.h
*/
#include "managers/HttpInverterManager.h"

#include <ArduinoJson.h>
#include <cstdlib>
#include <cstring>
#include <math.h>

#include "AppConfig.h"
#include "Logger.h"
#include "workingmode.h"

namespace {

// Pinos fixos por hardware da UART2 ligada ao módulo ESP-01 (só usada em
// SOLAX_V2). Ver Lusol.ino original (pin_rx/pin_tx sob #ifdef OLED).
const uint8_t kEsp01RxPin = 17;
const uint8_t kEsp01TxPin = 5;

// Idênticos aos originais de asyncHttpClient.ino.
const uint32_t kConnectionTimeoutMs = 30000;
const uint32_t kReceivingDataTimeoutMs = 10000;

// Tamanho do buffer usado para (de)serializar JSON, igual ao "root"
// DynamicJsonDocument(4096) global do firmware original (partilhado por
// todos os parsers).
const size_t kJsonDocSize = 4096;

// Réplica exata de strstr(X, Y, m, n) de asyncHttpClient.ino: procura Y (de
// comprimento n) dentro de X (de comprimento m) usando o algoritmo KMP, sem
// depender de terminador nulo em X (os dados TCP crus não estão garantidos
// como terminados em '\0'). Devolve NULL se não encontrar.
const char *kmpFind(const char *X, const char *Y, int m, int n) {
  if (*Y == '\0' || n == 0) return X;
  if (m == 0 || n > m) return nullptr;

  int next[n + 1];
  for (int i = 0; i < n + 1; i++) next[i] = 0;

  for (int i = 1; i < n; i++) {
    int j = next[i + 1];
    while (j > 0 && Y[j] != Y[i]) j = next[j];
    if (j > 0 || Y[j] == Y[i]) next[i + 1] = j + 1;
  }

  for (int i = 0, j = 0; i < m; i++) {
    if (*(X + i) == *(Y + j)) {
      if (++j == n) return (X + i - j + 1);
    } else if (j > 0) {
      j = next[j];
      i--;
    }
  }
  return nullptr;
}

}  // namespace

HttpInverterManager::HttpInverterManager() : espSerial_(2) {}

HttpInverterManager::~HttpInverterManager() {
  closeHttpClient(true);
}

void HttpInverterManager::begin() {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  resetMessageBuffer();
  shellySensor_ = 1;
  lastRequestMs_ = millis();
  masterPwmValue_ = 100;
  masterRequestsPwmDisable_ = false;

  // Campos que esta fonte reporta de facto neste modo (equivalente ao
  // antigo defineWebMonitorFields(config.wversion), mas calculado uma só
  // vez aqui em vez de num bitfield global). SLAVE_MODE é a única excepção:
  // depende do "wversion" que o mestre reportar em cada mensagem, por isso
  // é recalculado a cada parseMasterFreeDs() (ver masterFieldMask()).
  PowerReadingFields f;

  switch (cfg.mode) {
    case SOLAX_V2:
      // UART2 fixa por hardware para o módulo ESP-01 (não usa Wifi/HTTP).
      espSerial_.begin(115200, SERIAL_8N1, kEsp01RxPin, kEsp01TxPin);
      espSerial_.printf("SSID: %s\n", cfg.esp01Ssid);
      Logger::info("HttpInverterManager: Solax V2 por serie (UART2), SSID enviado ao ESP-01: %s\n", cfg.esp01Ssid);
      f.pv1 = f.pv2 = true;
      f.gridVoltage = f.solarWatts = f.inverterTemperature = true;
      f.solarWattsToday = f.gridWatts = f.gridCurrent = true;
      f.batteryWatts = f.batterySoc = true;
      break;

    case SOLAX_V2_LOCAL:
      if (cfg.solaxVersion == 2) {
        f.pv1 = f.pv2 = true;
        f.gridVoltage = f.solarWatts = f.inverterTemperature = true;
        f.solarWattsToday = f.gridWatts = f.gridCurrent = true;
        f.batteryWatts = f.batterySoc = true;
      } else {
        // Formato "v3": não reporta corrente/tensão de um medidor externo
        // nem dados de bateria (parseSolaxV3Local() nunca lhes atribui
        // valor - tal como o parseJsonv3local() original, que os deixa
        // comentados).
        f.pv1 = f.pv2 = true;
        f.gridVoltage = f.solarWatts = f.inverterTemperature = true;
        f.solarWattsToday = f.gridWatts = true;
      }
      break;

    case SOLAX_V1:
      f.pv1 = f.pv2 = true;
      f.gridVoltage = f.solarWatts = f.inverterTemperature = true;
      f.solarWattsToday = f.gridWatts = f.gridCurrent = true;
      f.batteryWatts = f.batterySoc = true;
      break;

    case WIBEEE:
      f.gridWatts = f.gridVoltage = f.gridCurrent = true;
      f.powerFactor = f.frequency = f.importActiveEnergy = true;
      f.solarWatts = true;
      break;

    case SHELLY_EM:
      // Máscara fixa e combinada (rede + inversor), tal como o
      // defineWebMonitorFields(SHELLY_EM) original: os dois sensores são
      // interrogados alternadamente (ver processHttpResponse()), mas a UI
      // sempre considerou ambos os grupos de campos "disponíveis" desde que
      // o modo Shelly EM esteja ativo.
      f.gridWatts = f.gridVoltage = true;
      f.importActiveEnergy = f.exportActiveEnergy = true;
      f.solarWatts = true;
      break;

    case FRONIUS_API:
      f.solarWatts = f.solarWattsToday = f.gridWatts = true;
      break;

    case SLAVE_MODE:
      // Ver comentário acima: dinâmico, recalculado por mensagem.
      break;

    default:
      break;
  }

  fields_ = f;

  Logger::info("HttpInverterManager: modo %d, fonte %s, intervalo %lu ms\n", cfg.mode, cfg.sourceIp,
               (unsigned long)cfg.pollIntervalMs);
}

void HttpInverterManager::loop() {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  if (cfg.mode == SOLAX_V2) {
    pollEsp01Serial();
    return;
  }

  if (responseReady_) {
    processHttpResponse();
    return;
  }

  if (client_) {
    if ((millis() - receivingDataStartMs_) > kReceivingDataTimeoutMs) {
      receivingData_ = false;
    }
    if ((millis() - connectionStartMs_) > kConnectionTimeoutMs && !receivingData_) {
      if (client_->connected()) {
        client_->close(true);
      } else {
        closeHttpClient(true);
      }
    }
    return;
  }

  if ((millis() - lastRequestMs_) < cfg.pollIntervalMs) {
    return;
  }
  lastRequestMs_ = millis();
  startHttpRequest();
}

// ---------------------------------------------------------------------------
// Solax V2 por série (UART2 <-> módulo ESP-01)
// ---------------------------------------------------------------------------

void HttpInverterManager::pollEsp01Serial() {
  if (!espSerial_.available()) return;

  String data = espSerial_.readStringUntil('\n');
  if (data.startsWith("{\"Data\":")) {
    parseEsp01Json(data);
  }
  Logger::info("Datos EPS01: %s\n", data.c_str());
}

void HttpInverterManager::parseEsp01Json(const String &json) {
  DynamicJsonDocument doc(kJsonDocSize);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Logger::info("HttpInverterManager: deserializeJson() ESP-01 falhou: %s\n", error.c_str());
    return;
  }

  PowerReading r = reading_;

  r.pv1Current = doc["Data"][0];           // Corriente string 1
  r.pv2Current = doc["Data"][1];           // Corriente string 2
  r.pv1Voltage = doc["Data"][2];           // Tension string 1
  r.pv2Voltage = doc["Data"][3];           // Tension string 2
  r.pv1Watts = doc["Data"][4];             // Potencia string 1
  r.pv2Watts = doc["Data"][5];             // Potencia string 2
  r.gridVoltage = doc["Data"][6];          // Tension de red
  r.solarWatts = doc["Data"][7];           // Potencia solar
  r.inverterTemperature = doc["Data"][8];  // Temperatura
  r.solarWattsToday = doc["Data"][9];      // Potencia solar diaria
  r.gridWatts = doc["Data"][10];           // Potencia de red (Negativo: de red - Positivo: a red)
  // Data[11] = wtogrid (potencia diaria enviada a red): sem campo
  // equivalente em PowerReading, descartado (tal como em todos os outros
  // modos Solax abaixo).
  // Data[12] era guardado em "meter.voltage" no original; PowerReading só
  // tem um único campo de tensão de rede, já usado acima (gridv), por isso
  // este valor (normalmente redundante com Data[6]) é descartado aqui.
  r.gridCurrent = doc["Data"][13];  // "meter.current" no original
  r.batteryWatts = doc["Data"][14];
  r.batterySoc = doc["Data"][15];

  if (ConfigStore::get().surplus.changeGridSign) {
    r.gridWatts *= -1.0f;
  }

  publishReading(r);
}

// ---------------------------------------------------------------------------
// Cliente TCP assíncrono cru (HTTP) - SOLAX_V2_LOCAL, SOLAX_V1, WIBEEE,
// SHELLY_EM, FRONIUS_API, SLAVE_MODE
// ---------------------------------------------------------------------------

void HttpInverterManager::startHttpRequest() {
  if (client_) return;  // ligação anterior ainda em curso

  client_ = new AsyncClient();
  if (!client_) return;

  connectionStartMs_ = millis();
  client_->setRxTimeout(5);  // sem timeout de RX (segundos) para a ligação

  client_->onError([this](void *, AsyncClient *client, int8_t) {
    receivingData_ = false;
    if (client->connected()) client->close(true);
  });

  client_->onTimeout([this](void *, AsyncClient *client, uint32_t) {
    receivingData_ = false;
    if (client->connected()) client->close(true);
  });

  client_->onDisconnect([this](void *, AsyncClient *client) {
    delete client;
    client_ = nullptr;
    onClientDisconnect();
  });

  client_->onConnect([this](void *, AsyncClient *client) { onClientConnect(client); });

  client_->onData([this](void *, AsyncClient *client, void *data, size_t len) {
    onClientData(client, (const char *)data, len);
  });

  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;
  if (!client_->connect(String(cfg.sourceIp).c_str(), 80)) {
    Logger::info("HttpInverterManager: falha ao ligar a %s\n", cfg.sourceIp);
    closeHttpClient(true);
  }
}

void HttpInverterManager::closeHttpClient(bool abortConnection) {
  if (!client_) return;
  receivingData_ = false;
  AsyncClient *c = client_;
  client_ = nullptr;
  if (abortConnection) {
    c->abort();
  }
  delete c;
}

void HttpInverterManager::resetMessageBuffer() {
  memset(messageBuffer_, 0, sizeof(messageBuffer_));
  messageLength_ = 0;
  totalMessageLength_ = 0;
  firstChunk_ = false;
  receivingDataStartMs_ = millis();
}

void HttpInverterManager::buildRequestLine(char *out, size_t outSize) const {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  switch (cfg.mode) {
    case SOLAX_V2_LOCAL:
      snprintf(out, outSize,
               "POST /?optType=ReadRealTimeData&pwd=admin HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n"
               "Content-Length: 0\r\nAccept: /*/\r\nContent-Type: application/x-www-form-urlencoded\r\n"
               "X-Requested-With: com.solaxcloud.starter\r\n\r\n",
               cfg.sourceIp);
      break;
    case SOLAX_V1:
      snprintf(out, outSize, "GET /api/realTimeData.htm HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", cfg.sourceIp);
      break;
    case WIBEEE:
      snprintf(out, outSize, "GET /en/status.xml HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", cfg.sourceIp);
      break;
    case SHELLY_EM:
      snprintf(out, outSize, "GET /emeter/%d HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", (int)(shellySensor_ - 1),
               cfg.sourceIp);
      break;
    case FRONIUS_API:
      snprintf(out, outSize,
               "GET /solar_api/v1/GetPowerFlowRealtimeData.fcgi HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n",
               cfg.sourceIp);
      break;
    case SLAVE_MODE:
      snprintf(out, outSize, "GET /masterdata HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", cfg.sourceIp);
      break;
    default:
      out[0] = '\0';
      break;
  }
}

void HttpInverterManager::onClientConnect(AsyncClient *client) {
  client->onError(nullptr, nullptr);

  char url[250];
  buildRequestLine(url, sizeof(url));

  if (client->space() > 32 && client->canSend()) {
    client->write(url);
  }
}

void HttpInverterManager::onClientData(AsyncClient *client, const char *d, size_t len) {
  char tmp[6] = {0};

  receivingData_ = true;
  receivingDataStartMs_ = millis();

  // Procura o cabeçalho "Content-Length:"
  const char *posContentLength = kmpFind(d, "Content-Length:", (int)len, 15);
  if (posContentLength != nullptr) {
    if (firstChunk_) { resetMessageBuffer(); }

    uint16_t contentLengthStart = (uint16_t)(posContentLength - d);
    const char *searchStart = posContentLength + 1;
    size_t remaining = len - (size_t)(searchStart - d);
    const void *cr = memchr(searchStart, '\r', remaining);
    uint16_t contentLengthStop = cr ? (uint16_t)((const char *)cr - d) : contentLengthStart;

    uint16_t arrayPos = 0;
    for (uint16_t i = contentLengthStart + 16; i < contentLengthStop && arrayPos < sizeof(tmp) - 1; i++) {
      tmp[arrayPos++] = d[i];
    }
    totalMessageLength_ = (uint16_t)atoi(tmp);
  }

  // Procura o fim dos cabeçalhos ("\r\n\r\n")
  const char *dataPayload = kmpFind(d, "\r\n\r\n", (int)len, 4);
  if (dataPayload != nullptr) {
    uint16_t payloadPos = (uint16_t)(dataPayload - d + 4);

    if (firstChunk_ && payloadPos < 256) { resetMessageBuffer(); }

    if (payloadPos < 256) {
      payloadStart_ = payloadPos;
      messageLength_ = (uint16_t)len - payloadStart_;
    }
  }

  if (dataPayload != nullptr && !firstChunk_) {
    // Primeiro chunk
    firstChunk_ = true;
    uint16_t arrayPos = 0;
    for (uint16_t i = payloadStart_; i < len; i++) {
      messageBuffer_[arrayPos++] = d[i];
    }
  } else {
    // Chunks seguintes
    uint16_t arrayPos = messageLength_;

    if (totalMessageLength_ < kMaxMessageSize && (messageLength_ + len) < kMaxMessageSize) {
      for (size_t i = 0; i < len; i++) {
        messageBuffer_[arrayPos++] = d[i];
      }
      messageBuffer_[arrayPos] = '\0';
      messageLength_ += (uint16_t)len;
    } else {
      resetMessageBuffer();
      receivingData_ = false;
      client->close();
      return;
    }
  }

  // Mensagem completa: fecha a ligação (o parsing acontece em
  // onClientDisconnect()/processHttpResponse(), fora deste callback).
  if (messageLength_ == totalMessageLength_) {
    firstChunk_ = false;
    client->close();
  }
}

void HttpInverterManager::onClientDisconnect() {
  receivingData_ = false;
  if (messageLength_ > 0) {
    responseReady_ = true;
  }
}

void HttpInverterManager::processHttpResponse() {
  const SurplusManagerConfig &cfg = ConfigStore::get().surplus;

  switch (cfg.mode) {
    case SOLAX_V2_LOCAL:
      if (cfg.solaxVersion == 2) { parseSolaxV2Local(messageBuffer_); }
      else { parseSolaxV3Local(messageBuffer_); }
      break;
    case SOLAX_V1:
      parseSolaxV1(messageBuffer_);
      break;
    case WIBEEE:
      parseWibeeeXml(messageBuffer_);
      break;
    case SHELLY_EM:
      parseShellyEm(messageBuffer_, shellySensor_);
      shellySensor_++;
      if (shellySensor_ > 2) { shellySensor_ = 1; }
      break;
    case SLAVE_MODE:
      parseMasterFreeDs(messageBuffer_);
      break;
    case FRONIUS_API:
      parseFronius(messageBuffer_);
      break;
    default:
      break;
  }

  resetMessageBuffer();
  responseReady_ = false;
  receivingData_ = false;
}

// ---------------------------------------------------------------------------
// Parsers - um por modo/fonte, campos e caminhos idênticos ao original
// ---------------------------------------------------------------------------

// Solax v2 local (JSON "Data": [...] formato "v2", solaxVersion == 2)
void HttpInverterManager::parseSolaxV2Local(const char *json) {
  DynamicJsonDocument doc(kJsonDocSize);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Logger::info("HttpInverterManager: deserializeJson() Solax v2 local falhou: %s\n", error.c_str());
    return;
  }

  PowerReading r = reading_;

  r.pv1Current = doc["Data"][0];
  r.pv2Current = doc["Data"][1];
  r.pv1Voltage = doc["Data"][2];
  r.pv2Voltage = doc["Data"][3];
  r.gridVoltage = doc["Data"][5];
  r.solarWatts = doc["Data"][6];
  r.inverterTemperature = doc["Data"][7];
  r.solarWattsToday = doc["Data"][8];
  r.gridWatts = doc["Data"][10];
  r.pv1Watts = doc["Data"][11];
  r.pv2Watts = doc["Data"][12];
  // Data[13] ("meter.voltage" no original) descartado - ver nota em
  // parseEsp01Json().
  r.gridCurrent = doc["Data"][14];  // "meter.current" no original
  r.batteryWatts = doc["Data"][15];
  r.batterySoc = doc["Data"][17];
  // Data[41] = wtogrid, sem campo equivalente, descartado.

  if (ConfigStore::get().surplus.changeGridSign) {
    r.gridWatts *= -1.0f;
  }

  publishReading(r);
}

// Solax v1 (idêntico ao v2 local, exceto a escala da temperatura). A API
// v1 da Solax por vezes devolve campos vazios no array como ",," (dois
// separadores consecutivos), o que não é JSON válido; tal como
// parseJsonv1() original, substitui-se por ",0," antes de interpretar.
void HttpInverterManager::parseSolaxV1(const char *json) {
  char fixedJson[380];
  uint16_t copyPos = 0;
  size_t len = strlen(json);

  for (size_t i = 0; i < len && copyPos < sizeof(fixedJson) - 2; i++) {
    fixedJson[copyPos++] = json[i];
    if (json[i] == ',' && i + 1 < len && json[i + 1] == ',') {
      fixedJson[copyPos++] = '0';
    }
  }
  fixedJson[copyPos] = '\0';

  DynamicJsonDocument doc(kJsonDocSize);
  DeserializationError error = deserializeJson(doc, fixedJson);

  if (error) {
    Logger::info("HttpInverterManager: deserializeJson() Solax v1 falhou: %s\n", error.c_str());
    return;
  }

  PowerReading r = reading_;

  r.pv1Current = doc["Data"][0];
  r.pv2Current = doc["Data"][1];
  r.pv1Voltage = doc["Data"][2];
  r.pv2Voltage = doc["Data"][3];
  r.gridVoltage = doc["Data"][5];
  r.solarWatts = doc["Data"][6];
  r.inverterTemperature = (float)doc["Data"][7] / 100.0f;
  r.solarWattsToday = doc["Data"][8];
  r.gridWatts = doc["Data"][10];
  r.pv1Watts = doc["Data"][11];
  r.pv2Watts = doc["Data"][12];
  r.gridCurrent = doc["Data"][14];  // "meter.current" no original
  r.batteryWatts = doc["Data"][15];
  r.batterySoc = doc["Data"][17];

  if (ConfigStore::get().surplus.changeGridSign) {
    r.gridWatts *= -1.0f;
  }

  publishReading(r);
}

// Solax v2 local, formato "v3" (solaxVersion != 2)
void HttpInverterManager::parseSolaxV3Local(const char *json) {
  DynamicJsonDocument doc(kJsonDocSize);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Logger::info("HttpInverterManager: deserializeJson() Solax v3 local falhou: %s\n", error.c_str());
    return;
  }

  PowerReading r = reading_;

  r.gridVoltage = (float)doc["Data"][0] / 10.0f;
  r.solarWatts = (float)doc["Data"][2];
  r.pv1Voltage = (float)doc["Data"][3] / 10.0f;
  r.pv2Voltage = (float)doc["Data"][4] / 10.0f;
  r.pv1Current = (float)doc["Data"][5] / 10.0f;
  r.pv2Current = (float)doc["Data"][6] / 10.0f;
  r.pv1Watts = (float)doc["Data"][7];
  r.pv2Watts = (float)doc["Data"][8];
  r.solarWattsToday = (float)doc["Data"][13] / 10.0f;
  r.inverterTemperature = (float)doc["Data"][39];
  r.gridWatts = (float)doc["Data"][48] - (float)doc["Data"][49];

  if (ConfigStore::get().surplus.changeGridSign) {
    r.gridWatts *= -1.0f;
  }

  publishReading(r);
}

// Wibeee (XML/texto simples, extraído com midString())
void HttpInverterManager::parseWibeeeXml(const char *xml) {
  String response = xml;
  PowerReading r = reading_;

  // Pinza 1 como lector de Red
  r.gridWatts = midString(response, "<fase1_p_activa>", "</fase1_p_activa>").toFloat();
  r.gridVoltage = midString(response, "<fase1_vrms>", "</fase1_vrms>").toFloat();
  r.gridCurrent = midString(response, "<fase1_irms>", "</fase1_irms>").toFloat();
  // O original também lê "<fase1_p_reactiva_ind>"/"<fase1_p_reactiva_cap>"
  // para meter.reactivePower (só para o mostrador, sem influência no sinal
  // nem noutro cálculo); sem campo equivalente em PowerReading, é omitido.

  float powerFactor = midString(response, "<fase1_factor_potencia>", "</fase1_factor_potencia>").toFloat();
  r.powerFactor = powerFactor;
  r.frequency = midString(response, "<fase1_frecuencia>", "</fase1_frecuencia>").toFloat();

  // Si powerFactor es negativo está volcando, positivo consumiendo.
  const bool changeGridSign = ConfigStore::get().surplus.changeGridSign;
  if (powerFactor > 0) {
    if (!changeGridSign) { r.gridWatts *= -1.0f; }
  } else {
    r.powerFactor *= -1.0f;
    if (changeGridSign) { r.gridWatts *= -1.0f; }
  }

  r.importActiveEnergy = midString(response, "<fase1_energia_activa>", "</fase1_energia_activa>").toFloat() / 1000.0f;

  // Pinza 2 como lector del Inversor
  r.solarWatts = midString(response, "<fase2_p_activa>", "</fase2_p_activa>").toFloat();

  publishReading(r);
}

// Shelly EM - sensor 1: /emeter/0 (medida de rede); sensor 2: /emeter/1
// (medida do inversor). Interrogados alternadamente por processHttpResponse().
void HttpInverterManager::parseShellyEm(const char *json, uint8_t sensor) {
  DynamicJsonDocument doc(kJsonDocSize);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Logger::info("HttpInverterManager: deserializeJson() Shelly EM falhou: %s\n", error.c_str());
    return;
  }

  if (doc["is_valid"] != true) {
    return;  // tal como o original: sem dados válidos, não há atualização
  }

  PowerReading r = reading_;

  if (sensor == 1) {  // Medida de Red
    r.gridWatts = roundf((float)doc["power"]);
    r.gridVoltage = roundf((float)doc["voltage"]);
    r.importActiveEnergy = roundf((float)doc["total"] / 1000.0f);
    r.exportActiveEnergy = roundf((float)doc["total_returned"] / 1000.0f);
    // "reactive" também é lido no original (meter.reactivePower) só para o
    // mostrador; sem campo equivalente em PowerReading, descartado.

    if (!ConfigStore::get().surplus.changeGridSign) {
      r.gridWatts *= -1.0f;
    }
  } else {  // Medida de Inversor
    r.solarWatts = roundf((float)doc["power"]);
  }

  publishReading(r);
}

// Fronius API
void HttpInverterManager::parseFronius(const char *json) {
  DynamicJsonDocument doc(kJsonDocSize);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Logger::info("HttpInverterManager: deserializeJson() Fronius falhou: %s\n", error.c_str());
    return;
  }

  PowerReading r = reading_;

  JsonVariant pv = doc["Body"]["Data"]["Site"]["P_PV"];
  JsonVariant day = doc["Body"]["Data"]["Site"]["E_Day"];
  JsonVariant grid = doc["Body"]["Data"]["Site"]["P_Grid"];

  r.solarWatts = (pv == "null") ? 0.0f : (float)pv;
  r.solarWattsToday = (day == "null") ? 0.0f : (float)day;
  r.gridWatts = (grid == "null") ? 0.0f : (float)grid;

  if (!ConfigStore::get().surplus.changeGridSign) {
    r.gridWatts *= -1.0f;
  }
  r.solarWattsToday = r.solarWattsToday / 1000.0f;  // w -> Kw, tal como o original

  publishReading(r);
}

// Lusol mestre (SLAVE_MODE) - GET /masterdata
void HttpInverterManager::parseMasterFreeDs(const char *json) {
  DynamicJsonDocument doc(kJsonDocSize);
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Logger::info("HttpInverterManager: deserializeJson() master Lusol falhou: %s\n", error.c_str());
    return;
  }

  uint8_t masterMode = (uint8_t)(int)doc["wversion"];
  bool tempShutdown = (bool)doc["tempShutdown"];
  masterPwmValue_ = tempShutdown ? 100 : (int)doc["PwmMaster"];
  masterRequestsPwmDisable_ = masterPwmValue_ < (int)ConfigStore::get().surplus.pwmSlaveOnPercent;

  PowerReadingFields mask;
  masterFieldMask(masterMode, mask);

  PowerReading r = reading_;
  const bool changeGridSign = ConfigStore::get().surplus.changeGridSign;

  if (mask.solarWatts) { r.solarWatts = (float)doc["wsolar"]; }
  if (mask.gridWatts) {
    r.gridWatts = (float)doc["wgrid"];
    if (changeGridSign) { r.gridWatts *= -1.0f; }
  }
  if (mask.inverterTemperature) { r.inverterTemperature = (float)doc["invTemp"]; }
  if (mask.batteryWatts) { r.batteryWatts = (float)doc["wbattery"]; }
  if (mask.batterySoc) { r.batterySoc = (float)doc["invSoC"]; }
  if (mask.loadWatts) { r.loadWatts = (float)doc["wload"]; }
  if (mask.solarWattsToday) { r.solarWattsToday = (float)doc["wtoday"]; }
  if (mask.gridVoltage) { r.gridVoltage = (float)doc["gridv"]; }
  if (mask.pv1) {
    r.pv1Current = (float)doc["pv1c"];
    r.pv1Voltage = (float)doc["pv1v"];
    r.pv1Watts = (float)doc["pw1"];
  }
  if (mask.pv2) {
    r.pv2Current = (float)doc["pv2c"];
    r.pv2Voltage = (float)doc["pv2v"];
    r.pv2Watts = (float)doc["pw2"];
  }
  // "mvoltage" (guardado como meter.voltage no original) descartado - ver
  // nota em parseEsp01Json(); "mcurrent" mapeia para gridCurrent, tal como
  // meter.current era o único sinal de corrente disponível.
  if (mask.gridCurrent) { r.gridCurrent = (float)doc["mcurrent"]; }
  if (mask.powerFactor) { r.powerFactor = (float)doc["mpowerFactor"]; }
  if (mask.frequency) { r.frequency = (float)doc["mfrequency"]; }
  if (mask.importActiveEnergy) { r.importActiveEnergy = (float)doc["mimportActive"]; }
  if (mask.exportActiveEnergy) { r.exportActiveEnergy = (float)doc["mexportActive"]; }

  fields_ = mask;
  publishReading(r);
}

// ---------------------------------------------------------------------------
// Utilitários
// ---------------------------------------------------------------------------

String HttpInverterManager::midString(const String &src, const char *startTag, const char *endTag) {
  int locStart = src.indexOf(startTag);
  if (locStart == -1) return "";
  locStart += strlen(startTag);
  int locFinish = src.indexOf(endTag, locStart);
  if (locFinish == -1) return "";
  return src.substring(locStart, locFinish);
}

// Réplica de defineWebMonitorFields() (Support_functions.ino), traduzida
// para os bits de PowerReadingFields que têm equivalente direto. Usada
// apenas para SLAVE_MODE, para saber que chaves do JSON "/masterdata" o
// mestre efetivamente preenche segundo o seu próprio "wversion".
void HttpInverterManager::masterFieldMask(uint8_t masterWversion, PowerReadingFields &out) {
  uint32_t mask;

  switch (masterWversion) {
    case SOLAX_V2:
    case SOLAX_V2_LOCAL:
    case SOLAX_V1:
      mask = 0x07FFE006;
      break;
    case MQTT_BROKER:
      mask = 0x0177E000;
      break;
    case ICC_SOLAR:
    case INGETEAM:
    case GOODWE:
      mask = 0x0F77E006;
      break;
    case WIBEEE:
    case WIBEEE_MODBUS:
      mask = 0x005801E6;
      break;
    case SHELLY_EM:
      mask = 0x00580322;
      break;
    case FRONIUS_API:
      mask = 0x00700000;
      break;
    case DDS238_METER:
      mask = 0x004003EF;
      break;
    case DDSU666_METER:
      mask = 0x004003EE;
      break;
    case SDM_METER:
      mask = 0x00401FFF;
      break;
    case SMA_BOY:
      mask = 0x0077E000;
      break;
    case SMA_ISLAND:
      mask = 0x07000006;
      break;
    case VICTRON:
      mask = 0x0E500006;
      break;
    case FRONIUS_MODBUS:
      mask = 0x0077E006;
      break;
    case HUAWEI_MODBUS:
      mask = 0x0377E000;
      break;
    case SOLAREDGE:
      mask = 0x0152A000;
      break;
    case SCHNEIDER:
      mask = 0x0B100006;
      break;
    default:
      mask = 0x0177E000;
      break;
  }

  out = PowerReadingFields();  // tudo a false

  // Posições de bit idênticas à union original (Support_functions.ino /
  // Lusol.ino). Bits sem campo equivalente em PowerReading (energyTotal,
  // "voltage" isolado do meter, activePower, aparentPower, reactivePower,
  // importReactive, exportReactive, phaseAngle, wtogrid) são ignorados de
  // propósito.
  out.gridCurrent = (mask & (1u << 2)) != 0;          // "current"   -> mcurrent
  out.powerFactor = (mask & (1u << 6)) != 0;           // "powerFactor" -> mpowerFactor
  out.frequency = (mask & (1u << 7)) != 0;             // "frequency" -> mfrequency
  out.importActiveEnergy = (mask & (1u << 8)) != 0;    // "importActive" -> mimportActive
  out.exportActiveEnergy = (mask & (1u << 9)) != 0;    // "exportActive" -> mexportActive
  out.pv1 = (mask & (1u << 13)) != 0;                  // "pv1c" (agrupado com pv1v/pw1)
  out.pv2 = (mask & (1u << 14)) != 0;                  // "pv2c" (agrupado com pv2v/pw2)
  out.gridVoltage = (mask & (1u << 19)) != 0;          // "gridv"
  out.solarWatts = (mask & (1u << 20)) != 0;           // "wsolar"
  out.solarWattsToday = (mask & (1u << 21)) != 0;      // "wtoday"
  out.gridWatts = (mask & (1u << 22)) != 0;            // "wgrid"
  out.inverterTemperature = (mask & (1u << 24)) != 0;  // "temperature"
  out.batteryWatts = (mask & (1u << 25)) != 0;         // "batteryWatts"
  out.batterySoc = (mask & (1u << 26)) != 0;           // "batterySoC"
  out.loadWatts = (mask & (1u << 27)) != 0;            // "loadWatts"
}
