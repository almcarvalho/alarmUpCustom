// Criado por Lucas Carvalho @br.lcsistemas
// HC-SR501 & ESP32 - 2026-02-02
// Versão com WiFiManager custom params + NTP + regras + reset config
// AJUSTE: permite tempo do relé = 0 (modo somente notificação)
// AJUSTE: força hora LOCAL (-03:00) via configTime(GMT_OFFSET_SEC) + getLocalTime()
//         Formato: 📢 Movimento detectado! Segunda-feira 16/02 às 14h12

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <time.h>

// ---- Pinos ----
const int pirPin = 27;       // PIR OUT
const int ledStatus = 2;     // LED indicador (status)
const int relePin = 25;      // RELÉ
const int btnResetPin = 0;   // BOOT (GPIO0)

// ---- Configurações default ----
unsigned long tempoAlarmeMs = 10000; // default 10s (configurável)
const unsigned long cooldownMs = 60000;
const unsigned long logIntervalMs = 700;

// ---- Anti-ruído / estabilidade ----
const unsigned long pirWarmupMs = 60000;
const unsigned long posReleIgnoreMs = 800;
const int amostrasPIR = 8;
const int amostrasMinHigh = 6;
const int delayAmostraMs = 25;

// ---- Estado ----
bool movimentoAtivo = false;
bool releAtivo = false;

unsigned long ultimoDisparo = 0;
unsigned long inicioRele = 0;
unsigned long ultimoLog = 0;

unsigned long inicioSistema = 0;
unsigned long ignorarPIRAte = 0;

// ---- Config persistente ----
Preferences prefs;

// Modos (Relé e Notificação)
enum ModoAcao : uint8_t {
  MODO_SEMPRE = 0,
  MODO_DIA = 1,
  MODO_NOITE = 2,
  MODO_FORA_COMERCIAL = 3,
  MODO_DENTRO_COMERCIAL = 4
};

struct Config {
  uint16_t releSegundos = 10; // 0 É PERMITIDO
  uint8_t  releModo = MODO_SEMPRE;

  char webhook[220] = ""; // Discord webhook pode ser grande
  uint8_t notifModo = MODO_SEMPRE;
};

Config cfg;

// ---- NTP / Timezone ----
// Bahia = UTC-3, sem DST -> FORÇAR offset -3h
static const long GMT_OFFSET_SEC = -3 * 3600;
static const int  DAYLIGHT_OFFSET_SEC = 0;

static const char* NTP1 = "pool.ntp.org";
static const char* NTP2 = "time.google.com";
static const char* NTP3 = "a.ntp.br";

// ---- Funções ----
void alerta();
bool pirPronto();
bool movimentoConfirmado();
void ligaRele(unsigned long agora);
void desligaRele(unsigned long agora);

bool timeReady();
void setupTimeNTP();
bool isDayTime(const tm& t);
bool isWithinBusinessHours(const tm& t);
bool modoPermite(uint8_t modo);

void saveConfig();
void loadConfig();
void clearConfigAndWifi();

bool buttonLongPressReset();

// ----------- helpers horário comercial -----------
static inline int toMin(int hh, int mm) { return hh * 60 + mm; }

bool isWithinBusinessHours(const tm& t) {
  // tm_wday: 0=Dom,1=Seg,...6=Sab
  int wday = t.tm_wday;
  int nowM = toMin(t.tm_hour, t.tm_min);

  // Domingo: nunca
  if (wday == 0) return false;

  // Segunda a Sexta (1..5)
  if (wday >= 1 && wday <= 5) {
    bool manha = (nowM >= toMin(7,50) && nowM <= toMin(12,15));
    bool tarde = (nowM >= toMin(14,00) && nowM <= toMin(18,20));
    return manha || tarde;
  }

  // Sábado (6): 07:50 - 13:00
  if (wday == 6) {
    return (nowM >= toMin(7,50) && nowM <= toMin(13,0));
  }

  return false;
}

// Dia/noite (definição simples)
// Dia=06:00-17:59, Noite=18:00-05:59
bool isDayTime(const tm& t) {
  int m = toMin(t.tm_hour, t.tm_min);
  return (m >= toMin(6,0) && m < toMin(18,0));
}

bool timeReady() {
  time_t now = time(nullptr);
  return now > 1700000000; // ~2023/2024 pra frente
}

// ---- Persistência ----
void loadConfig() {
  prefs.begin("alarme", true);
  cfg.releSegundos = prefs.getUShort("releS", 10);
  cfg.releModo = prefs.getUChar("releM", MODO_SEMPRE);
  cfg.notifModo = prefs.getUChar("notM", MODO_SEMPRE);

  String wh = prefs.getString("webhook", "");
  prefs.end();

  memset(cfg.webhook, 0, sizeof(cfg.webhook));
  if (wh.length() > 0) {
    wh.toCharArray(cfg.webhook, sizeof(cfg.webhook));
  }

  // aplica no runtime (0 permitido)
  tempoAlarmeMs = (unsigned long)cfg.releSegundos * 1000UL;
}

void saveConfig() {
  prefs.begin("alarme", false);
  prefs.putUShort("releS", cfg.releSegundos);
  prefs.putUChar("releM", cfg.releModo);
  prefs.putUChar("notM", cfg.notifModo);
  prefs.putString("webhook", String(cfg.webhook));
  prefs.end();

  // aplica no runtime (0 permitido)
  tempoAlarmeMs = (unsigned long)cfg.releSegundos * 1000UL;
}

void clearConfigAndWifi() {
  Serial.println("🧹 Limpando WiFiManager + Preferences...");

  prefs.begin("alarme", false);
  prefs.clear();
  prefs.end();

  WiFiManager wm;
  wm.resetSettings();

  delay(500);
  ESP.restart();
}

// ---- Reset via botão BOOT ----
bool buttonLongPressReset() {
  static bool lastPressed = false;
  static unsigned long t0 = 0;

  bool pressed = (digitalRead(btnResetPin) == LOW);

  if (pressed && !lastPressed) {
    t0 = millis();
  }
  if (!pressed) {
    t0 = 0;
  }

  lastPressed = pressed;

  if (pressed && t0 != 0 && (millis() - t0) >= 5000) {
    return true;
  }
  return false;
}

// ---- Helpers de formatação PT-BR ----
const char* nomeDiaSemanaPT(int wday) {
  // tm_wday: 0=Dom,1=Seg,...6=Sab
  switch (wday) {
    case 0: return "Domingo";
    case 1: return "Segunda-feira";
    case 2: return "Terça-feira";
    case 3: return "Quarta-feira";
    case 4: return "Quinta-feira";
    case 5: return "Sexta-feira";
    case 6: return "Sábado";
    default: return "";
  }
}

// Pega hora LOCAL de forma confiável (respeitando GMT_OFFSET_SEC)
bool getLocalTM(tm &out) {
  // getLocalTime aplica o offset configurado no configTime
  if (!getLocalTime(&out, 1500)) return false;
  return true;
}

String formatarDataHoraPT() {
  tm t{};
  if (!getLocalTM(t)) return String("");

  char buf[80];
  // Ex: "Segunda-feira 16/02 às 14h12"
  snprintf(buf, sizeof(buf), "%s %02d/%02d às %02dh%02d",
           nomeDiaSemanaPT(t.tm_wday),
           t.tm_mday, t.tm_mon + 1,
           t.tm_hour, t.tm_min);

  return String(buf);
}

bool modoPermite(uint8_t modo) {
  // Se não tem hora ainda:
  // - “sempre” permite
  // - demais modos bloqueiam até sincronizar
  if (!timeReady()) return (modo == MODO_SEMPRE);

  tm t{};
  if (!getLocalTM(t)) {
    // Se falhar pegar hora local, mantém a regra acima
    return (modo == MODO_SEMPRE);
  }

  bool dia = isDayTime(t);
  bool dentro = isWithinBusinessHours(t);

  switch (modo) {
    case MODO_SEMPRE: return true;
    case MODO_DIA: return dia;
    case MODO_NOITE: return !dia;
    case MODO_FORA_COMERCIAL: return !dentro;
    case MODO_DENTRO_COMERCIAL: return dentro;
    default: return true;
  }
}

// ---- NTP Setup ----
void setupTimeNTP() {
  Serial.println("🕒 Configurando NTP (offset -03:00)...");

  // AJUSTE PRINCIPAL:
  // Força offset -3 horas no sistema (Bahia) e usa NTP
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP1, NTP2, NTP3);

  unsigned long start = millis();
  while (!timeReady() && (millis() - start) < 10000) {
    delay(200);
  }

  tm t{};
  if (timeReady() && getLocalTM(t)) {
    Serial.print("✅ Hora sincronizada (local): ");
    Serial.printf("%04d-%02d-%02d %02d:%02d:%02d (wday=%d)\n",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec, t.tm_wday);
  } else {
    Serial.println("⚠️ Ainda sem hora (NTP). Modos por horário ficarão bloqueados exceto 'sempre'.");
  }
}

// ---- PIR ----
bool pirPronto() {
  return (millis() - inicioSistema) > pirWarmupMs;
}

bool movimentoConfirmado() {
  int contHigh = 0;
  for (int i = 0; i < amostrasPIR; i++) {
    if (digitalRead(pirPin) == HIGH) contHigh++;
    delay(delayAmostraMs);
  }
  Serial.print("Confirmacao PIR: ");
  Serial.print(contHigh);
  Serial.print("/");
  Serial.println(amostrasPIR);
  return contHigh >= amostrasMinHigh;
}

void ligaRele(unsigned long agora) {
  releAtivo = true;
  inicioRele = agora;
  digitalWrite(relePin, HIGH);
  ignorarPIRAte = agora + posReleIgnoreMs;
}

void desligaRele(unsigned long agora) {
  releAtivo = false;
  digitalWrite(relePin, LOW);
  Serial.println("⏱️ Relé desligado (tempo do alarme encerrou)");
  ignorarPIRAte = agora + posReleIgnoreMs;
}

// ---- Discord Webhook ----
void alerta() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado, nao foi possivel enviar alerta.");
    return;
  }

  if (strlen(cfg.webhook) < 10) {
    Serial.println("Webhook vazio/invalid. Configure no portal do WiFiManager.");
    return;
  }

  if (!modoPermite(cfg.notifModo)) {
    Serial.println("🔕 Notificacao bloqueada pelo modo configurado.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  Serial.println("[HTTPS] Enviando alerta Discord...");

  if (!https.begin(client, cfg.webhook)) {
    Serial.println("[HTTPS] Falha ao conectar (begin).");
    return;
  }

  https.addHeader("Content-Type", "application/json");

  // Mensagem no formato solicitado, com hora LOCAL (-03:00)
  String httpRequestData;
  if (timeReady()) {
    String quando = formatarDataHoraPT();
    if (quando.length() > 0) {
      httpRequestData = String("{\"content\":\"📢 Movimento detectado! ") + quando + "\"}";
    } else {
      httpRequestData = String("{\"content\":\"📢 Movimento detectado!\"}");
    }
  } else {
    httpRequestData = String("{\"content\":\"📢 Movimento detectado!\"}");
  }

  int httpCode = https.POST(httpRequestData);

  if (httpCode > 0) {
    Serial.printf("[HTTPS] Codigo da resposta: %d\n", httpCode);
  } else {
    Serial.printf("[HTTPS] Falha na requisicao: %s\n",
                  https.errorToString(httpCode).c_str());
  }

  https.end();
}

// -------------------- SETUP / LOOP --------------------
void setup() {
  pinMode(pirPin, INPUT_PULLDOWN);

  pinMode(ledStatus, OUTPUT);
  pinMode(relePin, OUTPUT);

  pinMode(btnResetPin, INPUT_PULLUP);

  digitalWrite(ledStatus, LOW);
  digitalWrite(relePin, LOW);

  Serial.begin(115200);
  delay(200);
  Serial.println("\nSensor de Presenca ESP32 (HC-SR501 + Rele) - Configuravel (WiFiManager + NTP)");

  inicioSistema = millis();

  // Carrega config salva (para pré-preencher portal)
  loadConfig();

  // ---- WiFiManager + parâmetros customizados ----
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);

  // Relé segundos (0 = não aciona)
  char releSegStr[6];
  snprintf(releSegStr, sizeof(releSegStr), "%u", (unsigned)cfg.releSegundos);
  WiFiManagerParameter p_rele_seg("rele_seg", "Rele: tempo aceso (segundos) - 0=nao acionar", releSegStr, 5);

  // Modo Relé (0..4)
  char releModoStr[3];
  snprintf(releModoStr, sizeof(releModoStr), "%u", (unsigned)cfg.releModo);
  WiFiManagerParameter p_rele_modo(
    "rele_modo",
    "Rele modo (0=sempre 1=dia 2=noite 3=fora_comercial 4=dentro_comercial)",
    releModoStr,
    2
  );

  // Webhook
  WiFiManagerParameter p_webhook("webhook", "Discord webhook (https://discord.com/api/webhooks/...)", cfg.webhook, 219);

  // Modo Notificação (0..4)
  char notifModoStr[3];
  snprintf(notifModoStr, sizeof(notifModoStr), "%u", (unsigned)cfg.notifModo);
  WiFiManagerParameter p_notif_modo(
    "notif_modo",
    "Notificacao modo (0=sempre 1=dia 2=noite 3=fora_comercial 4=dentro_comercial)",
    notifModoStr,
    2
  );

  wifiManager.addParameter(&p_rele_seg);
  wifiManager.addParameter(&p_rele_modo);
  wifiManager.addParameter(&p_webhook);
  wifiManager.addParameter(&p_notif_modo);

  // Forçar portal no boot segurando BOOT por ~1s (opcional)
  bool forcePortal = false;
  unsigned long t0 = millis();
  while (millis() - t0 < 1200) {
    if (digitalRead(btnResetPin) == LOW) { forcePortal = true; break; }
    delay(10);
  }

  if (forcePortal) {
    Serial.println("🔧 BOOT pressionado no inicio: abrindo portal WiFiManager...");
    wifiManager.startConfigPortal("ALARME", "1234567890");
  } else {
    if (!wifiManager.autoConnect("ALARME", "1234567890")) {
      Serial.println(F("Falha na conexao. Resetando..."));
      delay(1500);
      ESP.restart();
    }
  }

  Serial.println(F("Conectado no Wi-Fi."));
  Serial.print(F("IP: "));
  Serial.println(WiFi.localIP());

  // Lê valores do portal e salva
  uint16_t newReleS = (uint16_t)atoi(p_rele_seg.getValue());
  uint8_t newReleM = (uint8_t)atoi(p_rele_modo.getValue());
  uint8_t newNotM  = (uint8_t)atoi(p_notif_modo.getValue());

  // saneamento (0 permitido)
  if (newReleS > 600) newReleS = 600; // limite 10min
  if (newReleM > 4) newReleM = 0;
  if (newNotM > 4) newNotM = 0;

  cfg.releSegundos = newReleS;
  cfg.releModo = newReleM;
  cfg.notifModo = newNotM;

  // webhook
  memset(cfg.webhook, 0, sizeof(cfg.webhook));
  String wh = String(p_webhook.getValue());
  wh.trim();
  if (wh.length() > 0) {
    wh.toCharArray(cfg.webhook, sizeof(cfg.webhook));
  }

  saveConfig();

  digitalWrite(ledStatus, HIGH);

  // NTP (com offset -03:00)
  setupTimeNTP();

  Serial.print("Aguardando estabilizacao do PIR por ");
  Serial.print(pirWarmupMs / 1000);
  Serial.println("s...");
}

void loop() {
  // Reset config segurando BOOT por 5s
  if (buttonLongPressReset()) {
    Serial.println("🧨 BOOT segurado 5s: resetando configuracoes!");
    clearConfigAndWifi();
  }

  unsigned long agora = millis();

  // Warm-up do SR501
  if (!pirPronto()) {
    if ((agora / 500) % 2 == 0) digitalWrite(ledStatus, HIGH);
    else digitalWrite(ledStatus, LOW);
    delay(5);
    return;
  } else {
    digitalWrite(ledStatus, HIGH);
  }

  int pirState = digitalRead(pirPin);

  // Log periódico
  if (agora - ultimoLog >= logIntervalMs) {
    ultimoLog = agora;
    Serial.print("PIR: ");
    Serial.print(pirState == HIGH ? "HIGH" : "LOW");
    if (releAtivo) Serial.print(" | RELE: ON");
    Serial.println();
  }

  // Desliga relé depois do tempo configurado (se tempo > 0)
  if (releAtivo && tempoAlarmeMs > 0 && (agora - inicioRele >= tempoAlarmeMs)) {
    desligaRele(agora);
  }

  // período de ignorar PIR (anti-ruído)
  if (agora < ignorarPIRAte) {
    delay(5);
    return;
  }

  // Cooldown entre disparos
  bool podeDisparar = (agora - ultimoDisparo) >= cooldownMs;

  // Disparo por borda LOW->HIGH (com confirmação)
  if (pirState == HIGH && !movimentoAtivo && podeDisparar) {

    if (!movimentoConfirmado()) {
      movimentoAtivo = true;
      Serial.println("⚠️ Pico detectado (nao confirmado). Ignorado.");
    } else {
      movimentoAtivo = true;
      ultimoDisparo = agora;

      // Decide se PODE acionar relé conforme modo configurado
      if (modoPermite(cfg.releModo)) {

        if (cfg.releSegundos > 0) {
          ligaRele(agora);
          Serial.print("🚨 Movimento CONFIRMADO! Relé ligado por ");
          Serial.print(cfg.releSegundos);
          Serial.println("s.");
        } else {
          Serial.println("🚨 Movimento confirmado, mas tempo do relé = 0 (modo somente notificacao).");
        }

      } else {
        Serial.println("🚫 Movimento confirmado, mas RELÉ bloqueado pelo modo configurado.");
      }

      // Notificação respeita cfg.notifModo
      Serial.println("📣 Avaliando envio de alerta...");
      alerta();
    }
  }

  // Rearma quando PIR voltar a LOW
  if (pirState == LOW && movimentoAtivo) {
    movimentoAtivo = false;
    Serial.println("🔄 PIR voltou ao repouso. Re-armado.");
  }

  delay(10);
}
