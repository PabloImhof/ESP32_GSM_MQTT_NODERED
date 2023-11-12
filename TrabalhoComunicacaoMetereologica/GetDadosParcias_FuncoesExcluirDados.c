#include <Arduino.h>

#define SerialMon Serial
#define SerialAT Serial1
#define TINY_GSM_MODEM_SIM7000
#define TINY_GSM_RX_BUFFER 1024
#define GSM_PIN ""

const char apn[] = "zap.vivo.com.br";
const char gprsUser[] = "vivo";
const char gprsPass[] = "vivo";

const char *mqtt_broker = "xxxx";
const char *topic = "x";
const char *mqtt_username = "";
const char *mqtt_password = "";
const int mqtt_port = x;

const char *ssid = "x";
const char *password = "";

#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <FS.h>
#include <SD.h>

TinyGsm modem(SerialAT);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

#define uS_TO_S_FACTOR 1000000ULL
#define SLEEP_TIME 2 * 60 * uS_TO_S_FACTOR  // 2 minutos em microsegundos

// Defina os pinos para o módulo SIM7000
#define UART_BAUD 9600
#define PIN_DTR 25
#define PIN_TX 27
#define PIN_RX 26
#define PWR_PIN 4

// Defina os pinos para o cartão SD
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

// Definições das variáveis globais
const int botaoSimuladoPin = 12;
bool WifiConnected = false;
bool GPRSConnected = false;
String estacao_ID = "1";
String KeyValidator = "seu_codigo_verificador";
String antepenultima, penultimaLinha, ultimaLinha;
const char *caminhoDoArquivo = "/data.txt";
String Data_Leitura, Hora_Leitura, Chuvahora_mm, Chuva_dia_mm, Temperatura_C, Umidade, Pressao_Pa, T_Relva_C, Solar_W_m2, Umi_Solo, Vel_Vento_Km_H, Vel_Max_Vento_Km_h, Dir_Vento_Graus;
float porcentagemEspacoLivreSD;


const int MAX_WIFI_RETRIES = 3;    // Define o número máximo de tentativas de conexão WiFi
const int MAX_HTTP_RETRIES = 3;    // Define o número máximo de tentativas de conexão HTTP
const int MAX_GPRS_RETRIES = 3;    // Define o número máximo de tentativas de conexão GPRS
const int MAX_SDCARD_RETRIES = 3;  // Define o número máximo de tentativas de conexão SDCARD
const int MAX_MQTT_RETRIES = 3;    // Define o número máximo de tentativas de conexão MQTT
const int RETRY_DELAY = 10000;     // 10 segundos para tentar novamente as conexões
const int BUFFER_SIZE = 4096;      // Tamanho do buffer para leitura do arquivo

// Timeout para a rede em milissegundos
const int kNetworkTimeout = 10 * 60 * 1000;  // 10 minutos
const int kNetworkDelay = 1000;

// Nome do servidor que queremos conectar
const char kHostname[] = "192.168.4.1";
// Caminho para download (a parte da URL após o nome do host)
const char kPath[] = "/transferir?arquivo=/data.txt";


/* REGION SD CARD*/
bool IniciaSDCard() {
  for (int tentativas = 0; tentativas < MAX_SDCARD_RETRIES; tentativas++) {
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) {
      uint64_t usedSpace = SD.usedBytes();
      uint64_t totalBytes = SD.totalBytes();
      uint64_t freeSpace = totalBytes - usedSpace;
      porcentagemEspacoLivreSD = ((float)freeSpace / (float)totalBytes) * 100.0;
      Serial.print("Porcentagem de Espaço Livre: ");
      Serial.print(porcentagemEspacoLivreSD);
      Serial.println("%");
      return true;
    } else {
      Serial.println("Falha ao inicializar o cartão SD, tentativa " + String(tentativas + 1));
      delay(RETRY_DELAY);  // Espera um pouco antes de tentar novamente
    }
  }
  return false;
}

bool VerificaEspacoLivreSD(float limitePorcentagem) {
  if (porcentagemEspacoLivreSD < limitePorcentagem) {
    Serial.println("Espaço insuficiente no cartão SD. Limpando arquivos...");
    return LimpaSDCard();  // Retorna verdadeiro se conseguir limpar o espaço necessário
  }
  return true;  // Retorna verdadeiro se houver espaço suficiente
}

bool LimpaSDCard() {
  // Abre o diretório raiz do cartão SD
  File dir = SD.open("/");
  if (!dir) {
    Serial.println("Falha ao abrir diretório raiz para limpeza.");
    return false;
  }

  File arquivo;
  while (true) {
    arquivo = dir.openNextFile();
    if (!arquivo) {
      // Nenhum arquivo restante para excluir
      break;
    }

    String nomeArquivo = arquivo.name();
    arquivo.close();  // Feche o arquivo antes de tentar excluí-lo

    // Exclui o arquivo
    if (SD.remove(nomeArquivo.c_str())) {
      Serial.println("Arquivo removido: " + nomeArquivo);
    } else {
      Serial.println("Falha ao remover: " + nomeArquivo);
      return false;  // Retorna falso se algum arquivo não puder ser excluído
    }
  }
  // Depois de limpar, recalcule o espaço livre
  uint64_t usedSpace = SD.usedBytes();
  uint64_t totalBytes = SD.totalBytes();
  porcentagemEspacoLivreSD = 100.0 * (totalBytes - usedSpace) / totalBytes;

  return true;  // Retorna verdadeiro se a limpeza for bem-sucedida
}

bool lerTresUltimasLinhasSD(const char *path, String &linha1, String &linha2, String &linha3) {
  File file = SD.open(path);
  if (!file) {
    Serial.println("Erro ao abrir o arquivo!");
    return false;
  }

  const size_t bufferSize = 1024;  // Ajuste conforme necessário
  char buffer[bufferSize + 1];     // +1 para o caractere nulo

  long fileSize = file.size();
  long position;
  if (fileSize > bufferSize) {
    position = fileSize - bufferSize;
  } else {
    position = 0;
  }

  int linesFound = 0;

  while (position >= 0 && linesFound < 3) {
    file.seek(position);
    int bytesRead = file.readBytes(buffer, min(bufferSize, (size_t)(fileSize - position)));
    buffer[bytesRead] = '\0';  // Garante que o buffer seja uma string válida

    // Processa o buffer de trás para frente
    for (int i = bytesRead - 1; i >= 0; i--) {
      if (buffer[i] == '\n' || position + i == 0) {  // Nova linha ou início do arquivo
        String currentLine = String(buffer + i + 1);
        currentLine.trim();  // Remove espaços em branco e novas linhas extras

        if (linesFound == 0) linha3 = currentLine;
        else if (linesFound == 1) linha2 = currentLine;
        else if (linesFound == 2) {
          linha1 = currentLine;
          file.close();
          return true;  // Três linhas encontradas
        }
        linesFound++;
        buffer[i] = '\0';  // Termina a string atual no buffer
      }
    }
    position -= bufferSize;  // Move para o próximo bloco
  }

  file.close();
  return linesFound > 0;  // Retorna verdadeiro se pelo menos uma linha foi lida
}

/* END REGION SD CARD*/


bool ConectaWiFi() {
  WiFi.begin(ssid, password);
  for (int tentativas = 0; tentativas < MAX_WIFI_RETRIES; tentativas++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Conectado ao WiFi");
      return true;
    } else {
      simulaPressBotao();
      Serial.println("Conectando ao WiFi, tentativa " + String(tentativas + 1));
      delay(RETRY_DELAY);  // Espera um pouco antes de tentar novamente
    }
  }
  return false;
}

bool RealizaDownloadHTTP() {
  WiFiClient c;
  HttpClient http(c);

  for (int tentativas = 0; tentativas < MAX_HTTP_RETRIES; tentativas++) {
    http.get(kHostname, kPath);
    int statusCode = http.responseStatusCode();
    Serial.print("Código de status recebido: ");
    Serial.println(statusCode);
    statusCode = http.skipResponseHeaders();

    if (statusCode >= 0) {
      int bodyLen = http.contentLength();
      Serial.print("Tamanho do conteúdo: ");
      Serial.println(bodyLen);

      const long totalSizeToKeep = 10240;  // Últimos KB que você deseja manter
      long sizeToDiscard = 0;

      if (bodyLen > totalSizeToKeep) {
        sizeToDiscard = (bodyLen - totalSizeToKeep);
      }
      long discarded = 0;  // Quantos bytes já foram descartados

      Serial.print("sizeToDiscard ira descartar:  ");
      Serial.println(sizeToDiscard);

      File file = SD.open("/data.txt", FILE_WRITE);  // Abre o arquivo para escrita ou cria um novo se não existir
      if (file) {
        Serial.println("Arquivo aberto com sucesso.");
        uint8_t buffer[BUFFER_SIZE];
        int retornoBytes = 0;
        int totalWritten = 0;
        unsigned long timeoutStart = millis();

        while ((http.connected() || http.available()) && ((millis() - timeoutStart) < kNetworkTimeout)) {
          if (http.available()) {
            retornoBytes = http.read(buffer, BUFFER_SIZE);  // Lê os dados do servidor

            if (discarded + retornoBytes <= sizeToDiscard) {
              // Se ainda estamos apenas descartando, incrementamos 'discarded'
              discarded += retornoBytes;
              // Serial.print("Descartando: ");
              // Serial.println(discarded);
            } else {
              // Se já descartamos o suficiente, começamos a escrever
              if (discarded < sizeToDiscard) {
                // Calculamos quantos bytes precisamos descartar deste buffer
                int bytesToDiscardFromBuffer = sizeToDiscard - discarded;
                int bytesToWrite = retornoBytes - bytesToDiscardFromBuffer;
                file.write(buffer + bytesToDiscardFromBuffer, bytesToWrite);
                file.flush();  //o flush é chamdo junto ao close, porém não está funcionando fora do loop, então tive que forçar a gravação
                totalWritten += bytesToWrite;
                discarded = sizeToDiscard;  // Agora descartamos o suficiente
              } else {
                // Caso contrário, escrevemos todo o buffer
                file.write(buffer, retornoBytes);
                file.flush();
                totalWritten += retornoBytes;
              }
              Serial.println("Dados escritos no arquivo: " + String(totalWritten) + " bytes");  // Imprime a quantidade de bytes escritos
            }
            timeoutStart = millis();
          } else {
            delay(kNetworkDelay);
          }
        }
        delay(1000);

        file.flush();
        file.close();
        delay(1000);
        Serial.println("Download concluído.");
        // Serial.println("Tamanho do arquivo após download: " + String(file.size()) + " bytes");
        Serial.println("Total de bytes escritos: " + String(totalWritten) + " bytes");
      }
      return true;  // Download bem-sucedido
    } else {
      Serial.print("Falha ao obter a resposta: ");
      Serial.println(statusCode);
      http.stop();
      delay(RETRY_DELAY);  // Aguarda antes de tentar novamente
      continue;            // Isto irá imediatamente para a próxima iteração do for loop
    }

    http.stop();
    delay(RETRY_DELAY);  // Aguarda antes de tentar novamente
  }

  return false;  // Retorna falso se todas as tentativas falharem
}

// define a função para ligar o modem
void modemPowerOn() {
  pinMode(PWR_PIN, OUTPUT);
  digitalWrite(PWR_PIN, HIGH);
  delay(1000);
  digitalWrite(PWR_PIN, LOW);
}

bool PublicarDadosMQTT() {
  mqtt.setServer(mqtt_broker, mqtt_port);

  for (int attempt = 0; attempt < MAX_MQTT_RETRIES; attempt++) {
    if (mqtt.connect("ESP32Client")) {
      Serial.println(" mqtt broker conectado");

      String dados = "{\"CodigoVerificador\": \"" + KeyValidator + "\","
                                                                   "\"Estacao_ID\": "
                     + estacao_ID + ","
                                    "\"Data_Leitura\": \""
                     + Data_Leitura + "\","
                                      "\"Hora_Leitura\": \""
                     + Hora_Leitura + "\","
                                      "\"Chuva_hora_mm\": "
                     + Chuvahora_mm + ","
                                      "\"Chuva_dia_mm\": "
                     + Chuva_dia_mm + ","
                                      "\"Temperatura_C\": "
                     + Temperatura_C + ","
                                       "\"Umidade\": "
                     + Umidade + ","
                                 "\"Pressao_Pa\": "
                     + Pressao_Pa + ","
                                    "\"T_Relva_C\": "
                     + T_Relva_C + ","
                                   "\"Solar_W_m2\": "
                     + Solar_W_m2 + ","
                                    "\"Umi_Solo\": "
                     + Umi_Solo + ","
                                  "\"Vel_Vento_Km_H\": "
                     + Vel_Vento_Km_H + ","
                                        "\"Vel_Max_Vento_Km_h\": "
                     + Vel_Max_Vento_Km_h + ","
                                            "\"Dir_Vento_Graus\": "
                     + Dir_Vento_Graus + "}";

      mqtt.publish(topic, dados.c_str());
      return true;
    }
    attempt++;
    Serial.println("Tentativa falhou ao conectar ao broker MQTT. Tentando novamente...");

    delay(RETRY_DELAY);  // Espera antes de tentar novamente
  }

  return false;
}

void simulaPressBotao() {
  // Simula pressionar o botão após iniciar/acordar
  pinMode(botaoSimuladoPin, OUTPUT);
  digitalWrite(botaoSimuladoPin, LOW);
  delay(3000);  // Mantém "pressionado" por 3s
  // Solta o botão
  pinMode(botaoSimuladoPin, INPUT_PULLUP);
  delay(30000);  // Aguarda 30 segundos após a simulação do clique
}

void extraiDadosLinha(String linha) {
  linha.replace("\r", "");  // Remove o '\r' se estiver presente

  String values[13];
  int dataIndex = 0;
  int fieldIndex = 0;

  for (int i = 0; i < linha.length(); i++) {
    if (linha.charAt(i) == ';' || i == linha.length() - 1) {
      values[fieldIndex] = linha.substring(dataIndex, i + (i == linha.length() - 1 ? 1 : 0));
      dataIndex = i + 1;
      fieldIndex++;
    }
  }

  // Atribuição dos dados às variáveis globais
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
}



void setup() {
  SerialMon.begin(115200);
  delay(1000);  // Espere um pouco para que o monitor serial inicie corretamente
  Serial.println("Iniciando o Setup...");
  modemPowerOn();
  SerialAT.begin(UART_BAUD, SERIAL_8N1, PIN_RX, PIN_TX);

  Serial.println("Iniciando SDCARD...");
  if (!IniciaSDCard()) {
    Serial.println("Falha ao inicializar o cartão SD após várias tentativas, indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }

  Serial.println("Verificando espaço livre no cartão SD...");
  if (!VerificaEspacoLivreSD(40.0)) {
    Serial.println("Falha ao verificar o espaço livre após várias tentativas, indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }

  simulaPressBotao();
  Serial.println("Conectando ao WiFi...");
  if (!ConectaWiFi()) {
    Serial.println("Falha ao conectar no WiFi após várias tentativas, indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }

  Serial.println("Conectando GPRS Setup");
  for (int i = 0; i < MAX_GPRS_RETRIES; i++) {
    modem.init();  // Inicializa o modem
    // Tenta estabelecer conexão GPRS
    if (modem.gprsConnect(apn, gprsUser, gprsPass)) {
      GPRSConnected = true;
      Serial.println("Conexão GPRS bem sucedida!");
      break;  // Sai do loop se a conexão for bem-sucedida
    } else {
      // Se a conexão falhar, informa a tentativa e espera para tentar novamente
      SerialMon.printf("Tentativa %d falhou ao conectar via GPRS. Tentando novamente...\n", i + 1);
      delay(RETRY_DELAY);
    }
  }

  // Verifica se a conexão GPRS foi estabelecida após todas as tentativas
  if (!GPRSConnected) {
    Serial.println("Não foi possível conectar ao GPRS após todas as tentativas. Indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);  // Configura o tempo de espera para o modo de dormir
    esp_deep_sleep_start();                     // Coloca o dispositivo em modo de dormir profundo
  }
}

void loop() {
  Serial.println("Iniciando Loop...");
  Serial.println("Realizando o download...");
  if (!RealizaDownloadHTTP()) {
    Serial.println("Falha ao realizar o download após várias tentativas, indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  } else {
    Serial.println("Donwload e gravacao Ok");
  }

  if (lerTresUltimasLinhasSD(caminhoDoArquivo, antepenultima, penultimaLinha, ultimaLinha)) {
    if (antepenultima.length() > 0) {
      Serial.println("Antepenúltima linha: " + antepenultima);
      extraiDadosLinha(antepenultima);
      delay(1000);
      Serial.println("Publicando dados no broker MQTT...Penultima Linha");
      if (!PublicarDadosMQTT()) {
        Serial.println("Falha ao publicar dados no broker MQTT após várias tentativas, da Penultima linha");
      }
    }
    if (penultimaLinha.length() > 0) {
      Serial.println("Penúltima linha: " + penultimaLinha);
      extraiDadosLinha(penultimaLinha);
      delay(1000);
      Serial.println("Publicando dados no broker MQTT...Penultima Linha");
      if (!PublicarDadosMQTT()) {
        Serial.println("Falha ao publicar dados no broker MQTT após várias tentativas, da Penultima linha");
      }
    }
    if (ultimaLinha.length() > 0) {
      Serial.println("Última linha: " + ultimaLinha);
      extraiDadosLinha(ultimaLinha);
      delay(1000);
      Serial.println("Publicando dados no broker MQTT...Penultima Linha");
      if (!PublicarDadosMQTT()) {
        Serial.println("Falha ao publicar dados no broker MQTT após várias tentativas, da Penultima linha");
      }
    }
  } else {
    Serial.println("Falha ao ler as ultimas linhas do arquivo.");
  }


  Serial.println("Indo dormir...");
  esp_sleep_enable_timer_wakeup(SLEEP_TIME);
  esp_deep_sleep_start();
}
