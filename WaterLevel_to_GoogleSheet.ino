#include <WiFi.h>
#include <HTTPClient.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <esp_task_wdt.h>
#include <FS.h>
#include <SPIFFS.h>

// === CONFIGURAÇÕES DE HARDWARE ===
#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUTTON_PIN 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === WiFi e Script ===
const char* ssid = "SEU_WIFI";
const char* password = "SUA_SENHA";
const char* googleScriptURL = "https://script.google.com/macros/s/SEU_SCRIPT_ID/exec";
// === Deep Sleep ===
#define INTERVALO_HORA_US 3600000000ULL // 1 hora
//#define INTERVALO_HORA_US 300000000ULL // 5 MIN
RTC_DATA_ATTR int wakeCount = 0;

bool botaoEstavaApertado = false;

void setup() {
  Serial.begin(115200);
  esp_task_wdt_init(30, true);
  esp_task_wdt_add(NULL);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) while (true);
  display.clearDisplay(); display.display();

  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4, 0); // Wake por botão
  esp_sleep_enable_timer_wakeup(INTERVALO_HORA_US); // Wake por tempo

  if (!SPIFFS.begin(true)) {
    Serial.println("SPIFFS init failed");
    exibirMensagem("SPIFFS erro");
    delay(2000);
  }

  esp_sleep_wakeup_cause_t wakeSource = esp_sleep_get_wakeup_cause();

  conectarWiFi();

  if (wakeSource == ESP_SLEEP_WAKEUP_EXT0) {
    cicloManual();
  } else if (wakeSource == ESP_SLEEP_WAKEUP_TIMER) {
    sincronizarOffline();
    executarRegistro("automatica");
    dormir();
  } else {
    exibirMensagem("Inicializando...");
    delay(2000);
    dormir();
  }
}

void loop() {}

// === CICLO MANUAL (com exibição contínua) ===
void cicloManual() {
  botaoEstavaApertado = false;

  while (true) {
    esp_task_wdt_reset();
    bool botaoPressionado = digitalRead(BUTTON_PIN) == LOW;

    if (botaoPressionado) {
      float d = medirDistancia();
      mostrarDistancia(d);
      botaoEstavaApertado = true;
    } else {
      if (botaoEstavaApertado) {
        sincronizarOffline();               // 1. Envia pendentes
        executarRegistro("manual");         // 2. Envia nova medição
        SPIFFS.remove("/pendentes.txt");    // 3. Limpa pendentes
        Serial.println("pendentes.txt apagado após registro manual");
        break;
      }
    }
    delay(100);
  }

  dormir();
}

// === MEDIÇÃO COM SENSOR ULTRASSÔNICO ===
float medirDistancia() {
  digitalWrite(TRIG_PIN, LOW); delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(500);
  digitalWrite(TRIG_PIN, LOW);

  long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duracao == 0) return -1;
  return duracao * 0.0343 / 2;
}

// === MOSTRAR DISTÂNCIA NO DISPLAY ===
void mostrarDistancia(float dist) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  if (dist < 0) display.print("Erro na medicao");
  else {
    display.print("Distancia: ");
    display.print(dist, 1);
    display.print(" cm");
  }
  display.display();
}

// === MENSAGEM SIMPLES NO DISPLAY ===
void exibirMensagem(const char* texto) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print(texto);
  display.display();
}

// === REGISTRA MÉDIA DE 10 MEDIÇÕES VÁLIDAS ===
void executarRegistro(const char* tipo) {
  exibirMensagem("Medindo...");
  float soma = 0;
  int validas = 0, tentativas = 0;

  while (validas < 10 && tentativas < 20) {
    esp_task_wdt_reset();
    float d = medirDistancia();
    if (d > 0) {
      soma += d;
      validas++;
    }
    tentativas++;
    delay(100);
  }

  if (validas > 0) {
    float media = soma / validas;
    if (WiFi.status() == WL_CONNECTED) {
      enviarParaPlanilha(media, tipo);
    } else {
      salvarOffline(media, tipo);
    }
  } else {
    Serial.println("Sem medidas válidas");
    exibirMensagem("Erro ao medir");
    delay(1000);
  }
}

// === ENVIA DADO PARA PLANILHA (sem depender do código HTTP) ===
void enviarParaPlanilha(float media, const char* tipo) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Sem WiFi para envio");
    return;
  }

  HTTPClient http;
  String url = String(googleScriptURL) + "?distancia=" + String(media, 2) + "&tipo=" + tipo;
  http.begin(url);
  int code = http.GET();
  http.end();

  Serial.printf("Tentativa de envio (HTTP %d): %.2f [%s]\n", code, media, tipo);
  exibirMensagem("Registro OK");
  delay(1000);
}

// === SALVA MEDIÇÃO OFFLINE ===
void salvarOffline(float media, const char* tipo) {
  File f = SPIFFS.open("/pendentes.txt", FILE_APPEND);
  if (!f) return;
  unsigned long timestamp = millis();
  f.printf("%.2f;%s;%lu\n", media, tipo, timestamp);
  f.close();
  Serial.println("Salvo offline");
  exibirMensagem("Salvo offline");
  delay(1000);
}

// === TENTA SINCRONIZAR MEDIÇÕES ARMAZENADAS ===
void sincronizarOffline() {
  const char* originalPath = "/pendentes.txt";
  if (!SPIFFS.exists(originalPath)) return;

  File f = SPIFFS.open(originalPath, FILE_READ);
  if (!f) return;

  while (f.available()) {
    String linha = f.readStringUntil('\n');
    linha.trim();
    if (linha.length() < 5) continue;

    float media;
    char tipo[16];
    unsigned long ts;
    int campos = sscanf(linha.c_str(), "%f;%15[^;];%lu", &media, tipo, &ts);
    if (campos == 3) {
      enviarParaPlanilha(media, tipo);
    }

    esp_task_wdt_reset();
    delay(300);
  }

  f.close();
  SPIFFS.remove(originalPath); // Apaga tudo após tentativa
  Serial.println("pendentes.txt apagado após sincronização");
}

// === CONECTA AO WiFi ===
void conectarWiFi() {
  WiFi.begin(ssid, password);
  exibirMensagem("Conectando WiFi...");
  int tentativas = 0;
  while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
    delay(500);
    tentativas++;
    esp_task_wdt_reset();
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi OK");
    exibirMensagem("WiFi OK");
  } else {
    Serial.println("Falha WiFi");
    exibirMensagem("Sem WiFi");
    delay(1000);
  }
}

// === ENTRA EM DEEP SLEEP ===
void dormir() {
  exibirMensagem("Dormindo...");
  delay(1000);
  esp_deep_sleep_start();
}
