/*
  AppConfig.h - Configuração persistente da aplicação Lusol

  Substitui a antiga struct CONFIG monolítica (guardada em bruto na EEPROM,
  com uma escada de migrações manuais por versão) por uma configuração
  organizada em secções, serializada em JSON e guardada em "/config.json"
  no sistema de ficheiros interno. Campos novos assumem sempre um valor
  por omissão sensato, por isso não é necessária nenhuma migração manual.
*/
#pragma once

#include <Arduino.h>
#include <workingmode.h>
#include "Types.h"

struct NetworkConfig {
  bool dhcp = true;
  char ip[16] = "192.168.1.99";
  char gateway[16] = "192.168.1.1";
  char subnet[16] = "255.255.255.0";
  char dns1[16] = "8.8.8.8";
  char dns2[16] = "1.1.1.1";
  char ssid1[33] = "";
  char pass1[65] = "";
  char ssid2[33] = "";
  char pass2[65] = "";
  char hostname[24] = "";
  bool configured = false;
};

struct MqttConfig {
  bool enabled = false;
  char broker[64] = "192.168.1.2";
  uint16_t port = 1883;
  char user[32] = "";
  char password[32] = "";
  uint32_t publishIntervalMs = 10000;
  char relayTopic[4][64] = {"", "", "", ""};
  char solaxTopic[64] = "solax/tele/SENSOR";
  char meterTopic[64] = "meter/tele/SENSOR";
  char socTopic[64] = "inverter/battery/soc";
};

// Configuração do gestor de excedentes ativo (fonte de dados de rede/inversor)
struct SurplusManagerConfig {
  uint8_t mode = SOLAX_V2;
  char sourceIp[40] = "";          // IP do inversor/contador/gateway remoto
  char esp01Ssid[33] = "";         // Rede WiFi criada pelo módulo Solax ESP-01
  char esp01Password[65] = "";
  uint16_t meterBaud = 9600;       // Velocidade série do contador Modbus RTU
  uint8_t meterId = 1;             // Endereço Modbus do contador
  uint8_t solaxVersion = 2;
  bool changeGridSign = false;     // Inverte o sinal de import/export da rede
  bool useExternalMeter = false;
  bool useClamp = false;           // Usa a pinça amperimétrica local em vez de estimar a carga
  float clampCalibration = 22.3f;
  float clampVoltage = 230.0f;
  uint16_t attachedLoadWatts = 2000; // Potência nominal da carga ligada (estimativa sem pinça)
  bool useSolarAsMptt = false;
  bool useBmv = false;
  uint8_t gridPhase = 1;
  uint32_t maxErrorTimeMs = 20000; // Tempo máximo sem dados válidos antes de desligar por segurança
  uint32_t pollIntervalMs = 1500;  // Intervalo entre leituras (ajustado por gestor)
  uint16_t pwmSlaveOnPercent = 0;  // Limiar de % do Lusol mestre para ativar este escravo
};

struct PidConfig {
  float kp = 0.05f;
  float ki = 0.06f;
  float kd = 0.03f;
};

struct OffGridConfig {
  bool enabled = false;
  bool useVoltage = false;
  uint8_t socThreshold = 100;
  int16_t battWattsThreshold = -60;
  float batteryVoltage = 51.0f;
  float voltageOffset = 0.30f;
};

struct LoadControlConfig {
  bool pwmEnabled = true;
  bool manualMode = false;
  uint16_t potTarget = 60;             // Objetivo de potência de rede (W)
  uint16_t manualControlPercent = 50;
  uint16_t autoControlPercent = 60;
  uint16_t pwmFrequencyHz = 3000;
  bool dimmerLowCost = false;
  uint16_t maxPwmLowCost = 1073;
  uint16_t maxWattsTariff = 3450;      // Potência contratada, usada no modo manual
  PidConfig pid;
  RelayOutputConfig outputs[4];
  OffGridConfig offGrid;
  bool potManPwmActive = false;
  uint16_t potManPwmWatts = 0;         // Passa a manual automaticamente sob este valor de solar
  bool timerEnabled = false;
  uint16_t timerStartHHMM = 500;
  uint16_t timerStopHHMM = 700;
};

struct TemperatureConfig {
  bool enabled = false;
  uint8_t turnOnC = 55;
  uint8_t turnOffC = 65;
  uint8_t mode = 0; // 0: OFF, 1: apenas ligar, 2: apenas desligar, 3: liga+desliga
  uint8_t thermoSensorAddr[8] = {0};
  uint8_t triacSensorAddr[8] = {0};
  uint8_t customSensorAddr[8] = {0};
  char customSensorName[30] = "Temp. Ambiente";
};

struct SystemConfig {
  char language[6] = "pt";
  char timezone[48] = "WET0WEST,M3.5.0/1,M10.5.0";
  char ntpServer[40] = "pt.pool.ntp.org";
  bool oledPower = true;
  bool oledAutoOff = false;
  bool flipScreen = true;
  uint32_t oledAutoOffMs = 30000;
  uint8_t oledBrightness = 255;
  bool alexaControl = false;
  bool domoticzEnabled = false;
  uint16_t domoticzIdx[3] = {0, 0, 0};
  bool weblogEnabled = true;
  bool serialLogEnabled = true;
  uint8_t debugFlags = 0;
  char adminPasswordB64[48] = "YWRtaW4="; // "admin" em base64 (igual ao valor por omissão original)
};

struct EnergyTotals {
  float kwToday = 0;
  float kwExportToday = 0;
  float kwYesterday = 0;
  float kwExportYesterday = 0;
  float kwTotal = 0;
  float kwExportTotal = 0;
};

struct AppConfig {
  uint16_t schemaVersion = 2; // v2 = nova arquitetura (v1 era a EEPROM binária legada)
  NetworkConfig network;
  MqttConfig mqtt;
  SurplusManagerConfig surplus;
  LoadControlConfig load;
  TemperatureConfig temperature;
  SystemConfig system;
  EnergyTotals energy;
};

// Guarda/lê a AppConfig em "/config.json" (SPIFFS) e mantém uma cópia em
// memória acessível globalmente através de ConfigStore::get().
class ConfigStore {
public:
  static AppConfig &get();

  // Tenta carregar "/config.json"; se não existir ou estiver corrompido,
  // aplica valores por omissão e devolve false.
  static bool begin();

  static void save();
  static void resetToDefaults();

  // Backup/restauro para a interface web (exporta/importa o JSON completo)
  static String exportJson();
  static bool importJson(const String &json);

private:
  static void applyDefaults(AppConfig &cfg);
};
