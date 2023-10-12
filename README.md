# Estação Meteorológica com ESP32 LILYGO e SIM7000G

Este repositório contém o código-fonte para uma estação meteorológica baseada em ESP32 que utiliza o módulo GSM SIM7000G para comunicação. Os dados coletados pela estação são enviados para um broker MQTT via conexão GPRS.
Projeto e código desenvolvido para um projeto acadêmico no Curso de Engenharia de Computação.

## Características

- Utiliza o módulo GSM SIM7000G para conexão GPRS.
- Conecta-se a uma rede WiFi para download de dados.
- Publica os dados coletados para um broker MQTT.
- Pode ser configurado para trabalhar com SSL.
- O ESP32 é configurado para entrar em modo de economia de energia (deep sleep) entre as leituras.

## Configuração

### Constantes

- `apn`, `gprsUser`, `gprsPass`: Credenciais para a conexão GPRS.
- `mqtt_broker`, `topic`, `mqtt_username`, `mqtt_password`, `mqtt_port`: Configurações do broker MQTT.
- `ssid`, `password`: Credenciais da rede WiFi.

### Pinos

- `PIN_DTR`, `PIN_TX`, `PIN_RX`, `PWR_PIN`: Pinos usados para comunicação com o módulo SIM7000G.

## Como usar

1. Configure as constantes no início do código com suas próprias credenciais e configurações.
2. Modifique a biblioteca PubSubClient, variável MQTT_MAX_PACKET_SIZE para 512.
3. Carregue o código para o seu ESP32.
4. O ESP32 irá conectar-se à rede WiFi especificada e tentar baixar os dados da URL fornecida.
5. Após obter os dados, ele irá conectar-se via GPRS e publicar os dados no broker MQTT especificado.
6. O ESP32 entrará em modo de economia de energia (deep sleep) por um período especificado antes de acordar e repetir o processo.

## Funções

- `modemPowerOn()`: Liga o modem SIM7000.
- `disableGPS()`: Desativa o GPS.
- `PublicarDadosMQTT()`: Tenta conectar-se ao broker MQTT e publica os dados coletados.
- `DownloadDados()`: Conecta-se a uma URL via WiFi e baixa os dados meteorológicos.
- `DownloadDadosTesteLocal()`: Função de teste que conecta-se a uma API de CEP para fins de demonstração.

## Contribuições

Sinta-se à vontade para criar issues ou pull requests para melhorar este projeto.

## Licença

Este projeto está licenciado sob a licença MIT.

## Créditos

- [TinyGsmClient](https://github.com/vshymanskyy/TinyGSM): Uma biblioteca cliente para modems GSM.
- PubSubClient
- HTTPClient
- WiFi

---

**Nota**: Este é um documento básico README para o projeto baseado no código fornecido. Você pode adicionar mais detalhes, como imagens, diagramas, instruções de instalação detalhadas, etc., conforme necessário para o seu projeto.
