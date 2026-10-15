// Vakthund: pingar ein vert og sjekkar ein TCP-port kvart minutt,
// og varslar på Discord via webhook.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP32Ping.h>
#include "secrets.h"

#ifndef WATCH_HOST
#define WATCH_HOST "192.168.1.225"
#endif
#ifndef WATCH_PORT
#define WATCH_PORT 22
#endif

constexpr uint32_t CHECK_INTERVAL_MS = 60UL * 1000UL;
constexpr int FAILS_BEFORE_ALERT = 3;
constexpr uint32_t REMINDER_MS = 6UL * 60UL * 60UL * 1000UL;

static int failStreak = 0;
static bool alertSent = false;   // første varsel sendt (verten er "nede")
static bool recoveryPending = false;
static uint32_t lastCheck = 0;
static uint32_t lastAlertMs = 0;
static uint32_t downSinceMs = 0;
static String lastDetail;

static bool wifiOk() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.println("WiFi nede, koplar til att...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  uint32_t t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) delay(250);
  return WiFi.status() == WL_CONNECTED;
}

static String jsonEscape(const String &s) {
  String o;
  for (char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += c; }
    else if (c == '\n') o += "\\n";
    else o += c;
  }
  return o;
}

static bool sendDiscord(const String &msg) {
  WiFiClientSecure client;
  client.setInsecure();  // ingen sertifikatsjekk – enkelt, men sjå README
  HTTPClient http;
  if (!http.begin(client, DISCORD_WEBHOOK_URL)) return false;
  http.addHeader("Content-Type", "application/json");
  int code = http.POST("{\"content\":\"" + jsonEscape(msg) + "\"}");
  http.end();
  Serial.printf("Discord: HTTP %d\n", code);
  return code >= 200 && code < 300;
}

static String varighet(uint32_t ms) {
  uint32_t m = ms / 60000UL;
  char b[32];
  snprintf(b, sizeof b, "%luh %lum", (unsigned long)(m / 60), (unsigned long)(m % 60));
  return String(b);
}

static void sjekk() {
  bool ping = Ping.ping(WATCH_HOST, 2);
  WiFiClient c;
  bool tcp = c.connect(WATCH_HOST, WATCH_PORT, 3000);
  c.stop();
  Serial.printf("ping=%s tcp:%d=%s\n", ping ? "OK" : "FEIL", WATCH_PORT, tcp ? "OK" : "FEIL");

  if (ping && tcp) {
    failStreak = 0;
    if (alertSent || recoveryPending) {
      recoveryPending = true;
      String msg = String("✅ ") + WATCH_HOST + " svarer att (nede i ca " + varighet(millis() - downSinceMs) + ").";
      if (sendDiscord(msg)) { recoveryPending = false; alertSent = false; }
    }
    return;
  }

  lastDetail = String("ping ") + (ping ? "OK" : "FEIL") + ", TCP " + WATCH_PORT + " " + (tcp ? "OK" : "FEIL");
  if (++failStreak == FAILS_BEFORE_ALERT) downSinceMs = millis() - (FAILS_BEFORE_ALERT - 1) * CHECK_INTERVAL_MS;
  if (failStreak >= FAILS_BEFORE_ALERT) {
    if (!alertSent) {
      String msg = String("🚨 ") + WATCH_HOST + " svarer ikkje (" + failStreak + " feil på rad): " + lastDetail;
      if (sendDiscord(msg)) { alertSent = true; lastAlertMs = millis(); }
    } else if (millis() - lastAlertMs >= REMINDER_MS) {
      String msg = String("⏰ Påminning: ") + WATCH_HOST + " har vore nede i ca " + varighet(millis() - downSinceMs) + " (" + lastDetail + ").";
      if (sendDiscord(msg)) lastAlertMs = millis();
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.printf("Vakthund overvakar %s (ping + TCP %d)\n", WATCH_HOST, WATCH_PORT);
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  wifiOk();
  lastCheck = millis() - CHECK_INTERVAL_MS;  // sjekk med ein gong
}

void loop() {
  if (millis() - lastCheck >= CHECK_INTERVAL_MS) {
    lastCheck = millis();
    if (wifiOk()) sjekk();
    else Serial.println("Ingen WiFi – hoppar over sjekk (tel ikkje som vertsfeil).");
  }
  delay(100);
}
