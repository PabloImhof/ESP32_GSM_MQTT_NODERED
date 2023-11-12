// Inclua as bibliotecas necessárias
#include <SPI.h>
#include <WiFi.h>
#include <HttpClient.h>
#include <FS.h>
#include <SD.h>


// Defina os pinos para o cartão SD
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

#define uS_TO_S_FACTOR 1000000ULL
#define SLEEP_TIME 3 * 60 * uS_TO_S_FACTOR  // 3 minutos em microsegundos

// Defina os parâmetros do WiFi
const char* ssid = "Estacao_Ciclus";  // Substitua pelo seu SSID
const char* password = "";            // Substitua pela sua senha
float porcentagemEspacoLivreSD;
String penultimaLinha, ultimaLinha;

const int MAX_SDCARD_RETRIES = 3;  // Define o número máximo de tentativas de conexão SDCARD
const int MAX_WIFI_RETRIES = 3;    // Define o número máximo de tentativas de conexão WiFi
const int MAX_HTTP_RETRIES = 3;    // Define o número máximo de tentativas de conexão HTTP
const int RETRY_DELAY = 10000;     // 10 segundos para tentar novamente as conexões

const int BUFFER_SIZE = 4096;  // Tamanho do buffer para leitura do arquivo


// Timeout para a rede em milissegundos
const int kNetworkTimeout = 10 * 60 * 1000; // 10 minutos
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

      Serial.print("Tamanho do Cartão SD: ");
      Serial.print(totalBytes / (1024 * 1024));
      Serial.println("MB");

      Serial.print("Espaço Utilizado: ");
      Serial.print(usedSpace / (1024 * 1024));
      Serial.println("MB");

      Serial.print("Espaço Livre: ");
      Serial.print(freeSpace / (1024 * 1024));
      Serial.println("MB");

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

bool lerDuasUltimasLinhasSD(const char* path, String& penultimaLinha, String& ultimaLinha) {
  File file = SD.open(path);
  if (!file) {
    Serial.println("Erro ao abrir o arquivo!");
    return false;
  }

  // Define um buffer e uma variável para guardar o tamanho do arquivo
  const size_t bufferSize = 1024;  // Escolha um tamanho de buffer que seja um equilíbrio entre memória disponível e eficiência.
  char buffer[bufferSize];

  long fileSize = file.size();
  long position = fileSize;
  bool foundNewline = false;
  int bufferLength = 0;

  // Lê o arquivo de trás para frente em blocos
  while (position > 0) {
    size_t lenToRead = position < bufferSize ? position : bufferSize;
    position -= lenToRead;
    file.seek(position);
    bufferLength = file.readBytes(buffer, lenToRead);

    // Processa o buffer de trás para frente
    for (int i = bufferLength - 1; i >= 0; i--) {
      if (buffer[i] == '\n' || position + i == 0) {  // Se for o início do arquivo, trata como uma nova linha
        if (foundNewline) {
          penultimaLinha = ultimaLinha;
        }
        ultimaLinha = String(buffer + i + 1);          // Pula o caractere de nova linha
        if (position + i == 0 && buffer[0] != '\n') {  // Se estivermos no início do arquivo e o primeiro caractere não for uma nova linha
          penultimaLinha = ultimaLinha;                // A primeira linha é também a penúltima
        }
        foundNewline = true;
        buffer[i] = '\0';  // Termina a string atual no buffer
      }
      if (penultimaLinha.length() > 0 && ultimaLinha.length() > 0) {
        file.close();
        return true;  // Se já encontramos ambas as linhas, podemos sair
      }
    }
  }

  file.close();
  return ultimaLinha.length() > 0 && penultimaLinha.length() > 0;
}
/* END REGION SD CARD*/


bool ConectaWiFi() {
  WiFi.begin(ssid, password);
  for (int tentativas = 0; tentativas < MAX_WIFI_RETRIES; tentativas++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Conectado ao WiFi");
      return true;
    } else {
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
              Serial.print("Descartando: ");
              Serial.println(discarded);
            } else {
              // Se já descartamos o suficiente, começamos a escrever

              if (discarded < sizeToDiscard) {
                // Calculamos quantos bytes precisamos descartar deste buffer
                int bytesToDiscardFromBuffer = sizeToDiscard - discarded;
                int bytesToWrite = retornoBytes - bytesToDiscardFromBuffer;
                file.write(buffer + bytesToDiscardFromBuffer, bytesToWrite);
                file.flush();
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
        Serial.println("Tamanho do arquivo após download: " + String(file.size()) + " bytes");
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




void setup() {
  Serial.begin(115200);
  delay(1000);  // Espere um pouco para que o monitor serial inicie corretamente
  Serial.println("Iniciando o Setup...");

  Serial.println("Iniciando SDCARD...");
  if (!IniciaSDCard()) {
    Serial.println("Falha ao inicializar o cartão SD após várias tentativas, indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }

  Serial.println("Conectando ao WiFi...");
  if (!ConectaWiFi()) {
    Serial.println("Falha ao conectar no WiFi após várias tentativas, indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
  }

  Serial.println("Verificando espaço livre no cartão SD...");
  if (!VerificaEspacoLivreSD(20.0)) {
    Serial.println("Falha ao verificar o espaço livre após várias tentativas, indo dormir...");
    esp_sleep_enable_timer_wakeup(SLEEP_TIME);
    esp_deep_sleep_start();
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
    Serial.println("Donwload com suceeso");
  }

  Serial.println("Lendo as duas últimas linhas do arquivo...");
  if (lerDuasUltimasLinhasSD("/data.txt", penultimaLinha, ultimaLinha)) {
    Serial.println("Penúltima linha: " + penultimaLinha);
    Serial.println("Última linha: " + ultimaLinha);
  } else {
    Serial.println("Falha ao ler as duas últimas linhas do arquivo.");
  }

  Serial.println("Indo dormir...");
  esp_sleep_enable_timer_wakeup(SLEEP_TIME);
  esp_deep_sleep_start();
}
