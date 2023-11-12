// Inclua as bibliotecas necessárias
#include <SPI.h>
#include <FS.h>
#include <SD.h>

// Defina os pinos para o cartão SD
#define SD_MISO 2
#define SD_MOSI 15
#define SD_SCLK 14
#define SD_CS 13

#define uS_TO_S_FACTOR 1000000ULL
#define SLEEP_TIME 1 * 60 * uS_TO_S_FACTOR  // 3 minutos em microsegundos

String antepenultima, penultimaLinha, ultimaLinha;
const char* caminhoDoArquivo = "/data.txt";


const int MAX_SDCARD_RETRIES = 3;  // Define o número máximo de tentativas de conexão SDCARD
const int RETRY_DELAY = 10000;     // 10 segundos para tentar novamente as conexões
const int BUFFER_SIZE = 4096;      // Tamanho do buffer para leitura do arquivo


/* REGION SD CARD*/
bool IniciaSDCard() {
  for (int tentativas = 0; tentativas < MAX_SDCARD_RETRIES; tentativas++) {
    SPI.begin(SD_SCLK, SD_MISO, SD_MOSI, SD_CS);
    if (SD.begin(SD_CS)) {
      Serial.println("Cartão SD inicializado com sucesso.");
      return true;
    } else {
      Serial.println("Falha ao inicializar o cartão SD, tentativa " + String(tentativas + 1));
      delay(RETRY_DELAY);  // Espera um pouco antes de tentar novamente
    }
  }
  return false;
}


bool lerTresUltimasLinhasSD(const char* path, String& linha1, String& linha2, String& linha3) {
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
}

void loop() {
  Serial.println("Iniciando Loop...");

  Serial.println("Lendo as linhas do arquivo...");

  if (lerTresUltimasLinhasSD(caminhoDoArquivo, antepenultima, penultimaLinha, ultimaLinha)) {
    if (antepenultima.length() > 0) {
      Serial.println("Antepenúltima linha: " + antepenultima);
    }
    if (penultimaLinha.length() > 0) {
      Serial.println("Penúltima linha: " + penultimaLinha);
    }
    if (ultimaLinha.length() > 0) {
      Serial.println("Última linha: " + ultimaLinha);
    }
  } else {
    Serial.println("Falha ao ler as linhas do arquivo.");
  }

  Serial.println("Indo dormir...");
  esp_sleep_enable_timer_wakeup(SLEEP_TIME);
  esp_deep_sleep_start();
}
