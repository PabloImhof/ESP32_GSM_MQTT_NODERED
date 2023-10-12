#include <Arduino.h>

#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7000
// se estiver utilizando SSL, descomentar a linha abaixo, e comentar a linha acima
// #define TINY_GSM_MODEM_SIM7000SSL
#define TINY_GSM_RX_BUFFER 1024
#define GSM_PIN ""

const char apn[] = "zap.vivo.com.br";
const char gprsUser[] = "vivo";
const char gprsPass[] = "vivo";

const char *mqtt_broker = "x";
const char *topic = "x";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = x;

// const char *mqtt_broker = "test.mosquitto.org";
// const char *topic = "my/teste/imhof";
// // const char *mqtt_username = "";
// // const char *mqtt_password = "";
// const int mqtt_port = 1883;

const char *ssid = "SuaRede";
const char *password = "SuaSenha";

#include <TinyGsmClient.h>
#include <HTTPClient.h>
#include <PubSubClient.h>
#include <WiFi.h>

#ifdef DUMP_AT_COMMANDS
#include <StreamDebugger.h>
StreamDebugger debugger(SerialAT, SerialMon);
TinyGsm modem(debugger);
#else
TinyGsm modem(SerialAT);
#endif

TinyGsmClient client(modem);
// TinyGsmClientSecure client(modem);
PubSubClient mqtt(client);

#define uS_TO_S_FACTOR 1000000ULL
#define SLEEP_TIME 2 * 60 * uS_TO_S_FACTOR  // 2 minutos em microsegundos
#define UART_BAUD 9600
#define PIN_DTR 25
#define PIN_TX 27
#define PIN_RX 26
#define PWR_PIN 4

// Definições das variáveis globais
const int botaoSimuladoPin = 2;
bool WifiConnected = false;
bool GPRSConnected = false;
String estacao_ID = "1";
String KeyValidator = "KEY";
String Data_Leitura, Hora_Leitura, Chuvahora_mm, Chuva_dia_mm, Temperatura_C, Umidade, Pressao_Pa, T_Relva_C, Solar_W_m2, Umi_Solo, Vel_Vento_Km_H, Vel_Max_Vento_Km_h, Dir_Vento_Graus;

const int MAX_WIFI_RETRIES = 3;  // Define o número máximo de tentativas de conexão WiFi
const int MAX_GPRS_RETRIES = 3;  // Define o número máximo de tentativas de conexão GPRS
const int MAX_MQTT_RETRIES = 3;  // Define o número máximo de tentativas de conexão MQTT
const int RETRY_DELAY = 10000;   // 10 segundos para tentar novamente as conexões

// functions
// define a função para ligar o modem
void modemPowerOn() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(1000);
  digitalWrite(PWR_PIN, LOW);
}

// define a função para desligar o GPS
void disableGPS() {
  modem.sendAT("+CGPIO=0,48,1,0");
  if (modem.waitResponse(10000L) != 1) {
    Serial.println("Falha na configuração de potência do GPS BAIXA");
  }
  modem.disableGPS();
}

bool PublicarDadosMQTT() {
  mqtt.setServer(mqtt_broker, mqtt_port);

  for (int attempt = 0; attempt < MAX_MQTT_RETRIES; attempt++) {
    if (mqtt.connect("ESP32Client")) {
      Serial.println(" mqtt broker conectado");

      String dados = "{\"CodigoVerificador\": \"" + KeyValidator + "\","
                     "\"Estacao_ID\": " + estacao_ID + ","                     
                     "\"Data_Leitura\": \"" + Data_Leitura + "\","
                     "\"Hora_Leitura\": \"" + Hora_Leitura + "\","
                     "\"Chuva_hora_mm\": " + Chuvahora_mm + ","
                     "\"Chuva_dia_mm\": " + Chuva_dia_mm + ","
                     "\"Temperatura_C\": " + Temperatura_C + ","
                     "\"Umidade\": " + Umidade + ","
                     "\"Pressao_Pa\": " + Pressao_Pa + ","
                     "\"T_Relva_C\": " + T_Relva_C + ","
                     "\"Solar_W_m2\": " + Solar_W_m2 + ","
                     "\"Umi_Solo\": " + Umi_Solo + ","
                     "\"Vel_Vento_Km_H\": " + Vel_Vento_Km_H + ","
                     "\"Vel_Max_Vento_Km_h\": " + Vel_Max_Vento_Km_h + ","
                     "\"Dir_Vento_Graus\": " + Dir_Vento_Graus + "}";

      mqtt.publish(topic, dados.c_str());     
      return true;
    }
    attempt++;
    Serial.println("Tentativa falhou ao conectar ao broker MQTT. Tentando novamente...");

    delay(5000);  // Espera por 5 segundos antes de tentar novamente
  }

  return false;
}

bool DownloadDados() {
  HTTPClient http;
  http.begin("http://192.168.4.1/transferir?arquivo=/data.txt");  // original em produção

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Request bem sucedido");
    String payload = http.getString();
    int lastSemiColon = payload.lastIndexOf(';');

    if (lastSemiColon > 0) {
      int penultimateNewLine = payload.lastIndexOf('\n', lastSemiColon);
      String dataLine = payload.substring(penultimateNewLine + 1, lastSemiColon);

      // Removendo '\r' se presente
      dataLine.replace("\r", "");

      String values[13];
      int dataIndex = 0;
      int fieldIndex = 0;

      for (int i = 0; i < dataLine.length() && fieldIndex < 13; i++) {
        if (dataLine.charAt(i) == ';' || i == dataLine.length() - 1) {
          values[fieldIndex] = dataLine.substring(dataIndex, i);
          dataIndex = i + 1;
          fieldIndex++;
        }
      }

      Data_Leitura = values[0];
      Hora_Leitura = values[1];
      Chuvahora_mm = values[2];
      Chuva_dia_mm = values[3];
      Temperatura_C = values[4];
      Umidade = values[5];
      Pressao_Pa = values[6];
      T_Relva_C = values[7];
      Solar_W_m2 = values[8];
      Umi_Solo = values[9];
      Vel_Vento_Km_H = values[10];
      Vel_Max_Vento_Km_h = values[11];
      Dir_Vento_Graus = values[12];

      Serial.println("---- Dados Obtidos ----");
      Serial.println("Data Leitura: " + Data_Leitura);
      Serial.println("Hora Leitura: " + Hora_Leitura);
      Serial.println("Chuva por hora (mm): " + Chuvahora_mm);
      Serial.println("Chuva do dia (mm): " + Chuva_dia_mm);
      Serial.println("Temperatura (°C): " + Temperatura_C);
      Serial.println("Umidade: " + Umidade);
      Serial.println("Pressão (Pa): " + Pressao_Pa);
      Serial.println("Temperatura da relva (°C): " + T_Relva_C);
      Serial.println("Radiação solar (W/m^2): " + Solar_W_m2);
      Serial.println("Umidade do solo: " + Umi_Solo);
      Serial.println("Velocidade do vento (Km/H): " + Vel_Vento_Km_H);
      Serial.println("Velocidade máxima do vento (Km/h): " + Vel_Max_Vento_Km_h);
      Serial.println("Direção do vento (graus): " + Dir_Vento_Graus);
      Serial.println("------------------------");

      return true;
    }
  }
  Serial.println("Falha ao baixar dados.");
  return false;
}

bool DownloadDadosTesteLocal() {
  HTTPClient http;
  // Iniciar conexão com a URL
  http.begin("http://viacep.com.br/ws/98910000/json/");
  // Fazer a requisição GET
  int httpCode = http.GET();

  if (httpCode > 0)  // Verifica se a requisição foi bem-sucedida
  {
    String payload = http.getString();  // Obtém a resposta
    Serial.println(payload);

    // Para fins de simplificação e teste, apenas atribuir valores fixos
    Data_Leitura = "08/10/2023";
    Hora_Leitura = "18:49:00";
    Chuvahora_mm = "10";
    Chuva_dia_mm = "50";
    Temperatura_C = "25";
    Umidade = "70";
    Pressao_Pa = "101325";
    T_Relva_C = "20";
    Solar_W_m2 = "500";
    Umi_Solo = "15";
    Vel_Vento_Km_H = "10";
    Vel_Max_Vento_Km_h = "20";
    Dir_Vento_Graus = "45";

    http.end();
    return true;
  } else {
    Serial.println("Erro na requisição HTTP: " + String(httpCode));
    http.end();
    return false;
  }
}

void setup() {
  SerialMon.begin(115200);
  modemPowerOn();
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);
  disableGPS();
  Serial.println("Iniciando Setup");

  // Simula pressionar o botão após iniciar/acordar
  pinMode(botaoSimuladoPin, OUTPUT);
  digitalWrite(botaoSimuladoPin, LOW);
  delay(3000);  // Mantém "pressionado" por 3s
  // Solta o botão
  pinMode(botaoSimuladoPin, INPUT_PULLUP);
  delay(30000);  // Aguarda 30 segundos após a simulação do clique

  Serial.println("Conectando Wifi Setup");
  for (int i = 0; i < MAX_WIFI_RETRIES; i++) {
    WiFi.begin(ssid, password);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      WifiConnected = true;
      Serial.println("Conexão WIFI bem sucedida!");
      break;
    } else {
      SerialMon.printf("Tentativa %d falhou ao conectar no WiFi. Tentando novamente...\n", i + 1);
      delay(RETRY_DELAY);  // Aguarda antes de tentar novamente
    }
  }

  // Se após todas as tentativas ainda não estiver conectado wifi, vai dormir:
  if (!WifiConnected) {
    Serial.println("Não foi possível conectar ao WiFi após todas as tentativas. Indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }
  // se estiver utilizando SSL, descomentar a linha abaixo
  // client.setCertificate(root_ca);

  Serial.println("Conectando GPRS Setup");
  for (int i = 0; i < MAX_GPRS_RETRIES; i++) {
    modem.init();
    if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
      GPRSConnected = true;
      Serial.println("Conexão GPRS bem sucedida!");
      break;
    } else {
      SerialMon.printf("Tentativa %d falhou ao conectar via GPRS. Tentando novamente...\n", i + 1);
      delay(RETRY_DELAY);  // Aguarda antes de tentar novamente
    }
  }

  // Se após todas as tentativas ainda não estiver conectado gprs, vai dormir:
  if (!GPRSConnected) {
    Serial.println("Não foi possível conectar ao GPRS após todas as tentativas. Indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }
}

void loop() {
  Serial.println("Entrando no Loop");

  // Se a conexão WiFi foi bem-sucedida, prossegue com o download dos dados
  if (WifiConnected) {
    // if (DownloadDados())
    if (DownloadDadosTesteLocal()) {
      Serial.println("Dados obtidos com sucesso!");
      // Se a conexão GPRS foi bem-sucedida, prossegue com a publicação de dados via MQTT
      if (GPRSConnected) {
        if (PublicarDadosMQTT()) {
          Serial.println("Dados publicados com sucesso!");
        } else {
          Serial.println("Falha ao publicar os dados via MQTT após todas as tentativas. Indo dormir...");
        }
        esp_sleep_enable_timer_wakeup(SLEEP_TIME);
        esp_deep_sleep_start();
      } else {
        Serial.println("Falha ao conectar via GPRS após várias tentativas.");
      }
    } else {
      Serial.println("Falha ao baixar dados. Indo dormir...");
      esp_sleep_enable_timer_wakeup(SLEEP_TIME);
      esp_deep_sleep_start();
    }
  } else {
    Serial.println("WiFi não conectado após várias tentativas. Indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }
}
