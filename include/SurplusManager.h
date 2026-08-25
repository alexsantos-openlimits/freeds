/*
  SurplusManager.h - Interface comum a todos os gestores de excedentes

  Um "gestor de excedentes" (SurplusManager) é o módulo responsável por obter
  periodicamente os dados de potência de uma fonte concreta (contador Modbus,
  inversor por HTTP, MQTT, Modbus TCP, UDP, etc.) e traduzi-los para uma
  PowerReading comum, independente do protocolo de origem.

  Esta interface substitui o antigo switch/case espalhado por vários
  ficheiros .ino (getSensorData(), setGetDataTime(), defineWebMonitorFields())
  por uma fábrica única (SurplusManagerFactory) que instancia o gestor
  correto de acordo com o modo configurado (SurplusManagerConfig::mode).
*/
#pragma once

#include <Arduino.h>
#include "AppConfig.h"
#include "Types.h"

class SurplusManager {
public:
  virtual ~SurplusManager() {}

  // Chamado uma vez, com WiFi já ligado, antes do primeiro loop().
  virtual void begin() = 0;

  // Chamado no loop principal tão frequentemente quanto possível.
  // Cada implementação decide internamente quando é a hora de fazer o
  // próximo pedido, usando SurplusManagerConfig::pollIntervalMs.
  virtual void loop() = 0;

  // true a partir do momento em que exista pelo menos uma leitura válida
  // e a fonte não tenha excedido SurplusManagerConfig::maxErrorTimeMs desde
  // a última resposta.
  virtual bool isConnected() const = 0;

  virtual const PowerReading &reading() const = 0;
  virtual const PowerReadingFields &fields() const = 0;

  // Nome curto para logs/interface ("Solax V2", "Modbus DDS238", ...)
  virtual const char *name() const = 0;
};

// Funcionalidade comum a quase todas as implementações: guarda a última
// leitura, o instante da última atualização e o instante da última alteração
// significativa de wgrid (necessário para detetar leituras "coladas").
class SurplusManagerBase : public SurplusManager {
public:
  bool isConnected() const override {
    return lastUpdateMs_ != 0 && (millis() - lastUpdateMs_) < ConfigStore::get().surplus.maxErrorTimeMs;
  }
  const PowerReading &reading() const override { return reading_; }
  const PowerReadingFields &fields() const override { return fields_; }

protected:
  void publishReading(const PowerReading &r) {
    reading_ = r;
    lastUpdateMs_ = millis();
  }

  PowerReading reading_;
  PowerReadingFields fields_;
  unsigned long lastUpdateMs_ = 0;
};
