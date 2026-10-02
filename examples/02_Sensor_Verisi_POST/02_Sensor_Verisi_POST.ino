/*
 * İLİMERA LTE Bridge — 02 Sensör Verisini HTTPS POST ile Göndermek (ESP32 + ArduinoJson)
 *
 * Her 60 saniyede bir ölçümü JSON gövdeli POST olarak sunucuya gönderir ve
 * kılavuzdaki yeniden deneme kuralını uygular:
 *   - "ok":true                → başarılı
 *   - "ok":false + "status"    → sunucu cevap verdi ve reddetti: TEKRAR DENEME
 *   - "ok":false, "status" yok → sunucuya ulaşılamadı ("err"): 10 sn sonra tekrar dene
 *   - "acc":false, "err":"busy"→ kuyruk dolu: biraz sonra tekrar dene
 *   - {"evt":"ready"}          → köprü yeniden başladı: bekleyen istek kayboldu, yeniden gönder
 *   - {"evt":"fuota",...}      → modem güncelleniyor: "done" gelene kadar istek gönderme
 *
 * Gereken kütüphane: ArduinoJson (Benoit Blanchon, v7) — Kütüphane Yöneticisi'nden kurun.
 * Bağlantı: köprü TX → GPIO16, köprü RX ← GPIO17, GND ortak; 115200 8N1, 3.3 V.
 *
 * EN: POSTs a JSON reading every 60 s and applies the retry rules from the user guide
 *     (no retry when the server answered, retry when it was unreachable, resend after
 *     a bridge restart, pause during modem firmware updates). Requires ArduinoJson v7.
 */

#include <Arduino.h>
#include <ArduinoJson.h>

// ─────────── KULLANICI AYARLARI ───────────
#define KOPRU_RX_PIN 16
#define KOPRU_TX_PIN 17
#define POST_URL     "https://httpbin.org/post"
#define API_ANAHTARI "Bearer abc123"           // sunucunuz istemiyorsa boş bırakın
#define ANALOG_PIN   34
const uint32_t OLCUM_ARALIGI_MS = 60000;
const uint32_t TEKRAR_BEKLEME_MS = 10000;
const uint8_t  EN_FAZLA_DENEME = 5;
// ──────────────────────────────────────────

HardwareSerial kopru(2);
String satir;

enum Durum { BOSTA, CEVAP_BEKLENIYOR, TEKRAR_BEKLENIYOR };
Durum durum = BOSTA;
bool modemGuncelleniyor = false;
uint32_t istekNo = 0, beklenenId = 0, zaman = 0, sonOlcum = 0;
uint8_t deneme = 0;
String bekleyenGovde;            // tekrar gönderim için son ölçüm

void istekGonder() {
  JsonDocument istek;
  istek["cmd"] = "req";
  istek["id"] = beklenenId = ++istekNo;
  istek["method"] = "POST";
  istek["url"] = POST_URL;
  if (strlen(API_ANAHTARI)) istek["headers"]["Authorization"] = API_ANAHTARI;
  istek["body"] = serialized(bekleyenGovde);   // nesne olarak gömülür, köprü JSON olarak gönderir
  istek["timeout"] = 15000;

  String s;
  serializeJson(istek, s);                     // tek satır, en çok 2048 bayt
  Serial.println(">> " + s);
  kopru.print(s);
  kopru.print('\n');
  durum = CEVAP_BEKLENIYOR;
  zaman = millis();
  deneme++;
}

void tekrarPlanla(const char* sebep) {
  if (deneme >= EN_FAZLA_DENEME) {
    Serial.printf("[HATA] %s — %u denemeden sonra vazgecildi.\n", sebep, deneme);
    durum = BOSTA;
    return;
  }
  Serial.printf("[UYARI] %s — %lu sn sonra tekrar denenecek.\n", sebep, TEKRAR_BEKLEME_MS / 1000);
  durum = TEKRAR_BEKLENIYOR;
  zaman = millis();
}

void satirGeldi(const String& s) {
  Serial.println("<< " + s);
  JsonDocument m;
  if (deserializeJson(m, s)) return;           // JSON değilse yok say

  if (m["evt"].is<const char*>()) {
    String evt = m["evt"].as<String>();
    if (evt == "ready" && durum == CEVAP_BEKLENIYOR) tekrarPlanla("Kopru yeniden basladi, istek kayboldu");
    if (evt == "fuota") modemGuncelleniyor = (m["phase"].as<String>() == "start");
    return;
  }

  if (m["id"].as<uint32_t>() != beklenenId || durum != CEVAP_BEKLENIYOR) return;

  if (m["acc"].is<bool>()) {                   // birinci satır
    if (!m["acc"].as<bool>()) tekrarPlanla(m["err"] | "istek kabul edilmedi");
    return;
  }

  if (m["ok"].as<bool>()) {                    // ikinci satır: sonuç
    Serial.printf("Gonderildi (HTTP %d)\n", m["status"].as<int>());
    durum = BOSTA;
  } else if (m["status"].is<int>()) {
    Serial.printf("[HATA] Sunucu reddetti (HTTP %d) — tekrar denenmeyecek.\n", m["status"].as<int>());
    durum = BOSTA;
  } else {
    String err = m["err"] | "bilinmiyor";
    if (err == "parse" || err == "toolong" || err == "cfg") { Serial.println("[HATA] Istek hatali: " + err); durum = BOSTA; }
    else tekrarPlanla(("Sunucuya ulasilamadi: " + err).c_str());
  }
}

void setup() {
  Serial.begin(115200);
  kopru.begin(115200, SERIAL_8N1, KOPRU_RX_PIN, KOPRU_TX_PIN);
  delay(300);
  Serial.println("\n=== LTE Bridge sensor verisi POST ===");
}

void loop() {
  while (kopru.available()) {
    char c = kopru.read();
    if (c == '\n') { satir.trim(); if (satir.length()) satirGeldi(satir); satir = ""; }
    else if (c != '\r' && satir.length() < 4600) satir += c;
  }

  if (modemGuncelleniyor) return;              // güncelleme bitene kadar bekle

  switch (durum) {
    case BOSTA:
      if (sonOlcum == 0 || millis() - sonOlcum >= OLCUM_ARALIGI_MS) {
        sonOlcum = millis();
        JsonDocument g;
        g["cihaz"] = "ilimera-ornek";
        g["calisma_suresi_s"] = millis() / 1000;
        g["analog_mv"] = analogReadMilliVolts(ANALOG_PIN);
        bekleyenGovde = "";
        serializeJson(g, bekleyenGovde);
        deneme = 0;
        istekGonder();
      }
      break;
    case CEVAP_BEKLENIYOR:
      if (millis() - zaman > 40000) tekrarPlanla("Cevap gelmedi");   // köprü tavanı 30 sn + pay
      break;
    case TEKRAR_BEKLENIYOR:
      if (millis() - zaman >= TEKRAR_BEKLEME_MS) istekGonder();
      break;
  }
}
