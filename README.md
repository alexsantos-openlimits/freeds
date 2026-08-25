<h1 align="center">Lusol</h1>

<p align="center">Gestor de excedentes fotovoltaicos para ESP32.</p>

---

## O que é

O **Lusol** é um gestor de excedentes de energia (fotovoltaica e/ou outras fontes), que desvia a energia excedente da rede para cargas resistivas (ex.: termoacumulador) em vez de a injetar na rede elétrica. Corre num ESP32 (Heltec WiFi Kit 32), com ecrã OLED, 4 saídas de relé, saída PWM/DAC, pinça amperimétrica opcional e sensores de temperatura.

Suporta várias fontes de leitura de excedente (contadores Modbus RTU/TCP, inversores por HTTP, MQTT, GoodWe UDP, modo escravo), controlo por PID/histerese com estagiamento de relés, e integração automática com o **Home Assistant** via MQTT Discovery.

## Funcionalidades

- Interface web moderna (SPA), em português e inglês, com painel de configuração dedicado.
- Vários gestores de excedentes: Modbus RTU/TCP, HTTP (Solax, Wibeee, Shelly, Fronius), MQTT, GoodWe UDP, modo escravo.
- Controlo de carga por PID + histerese, com 4 relés de estágio e saída PWM/DAC.
- Configuração de rede Wi-Fi com fallback automático para modo ponto de acesso.
- MQTT com Last Will and Testament e descoberta automática de entidades no Home Assistant (MQTT Discovery), sem necessidade de integrações externas.
- Contabilização de energia e temperatura, com histórico e leitura por sensores DS18B20.
- Atualização de firmware via OTA.

## Compilar e gravar

Projeto em [PlatformIO](https://platformio.org/) (VSCode + extensão PlatformIO).

1. Clonar o repositório.
2. Abrir a pasta no VSCode com a extensão PlatformIO instalada.
3. `PlatformIO: Build` para compilar o firmware.
4. `PlatformIO: Upload` para gravar o firmware no ESP32.
5. `PlatformIO: Upload Filesystem Image` para gravar a interface web (SPIFFS).

Depois de gravado, liga-te à rede Wi-Fi `Lusol` criada pelo dispositivo e acede a `http://192.168.4.1` para configurar a rede Wi-Fi definitiva.

## Dependências

- [ArduinoJson](https://github.com/bblanchon/ArduinoJson)
- [AsyncMqttClient](https://github.com/marvinroger/async-mqtt-client)
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [ThingPulse OLED SSD1306](https://github.com/ThingPulse/esp8266-oled-ssd1306)
- [esp32ModbusTCP](https://github.com/bertmelis/esp32ModbusTCP)
- [Arduino PID Library](https://github.com/br3ttb/Arduino-PID-Library)
