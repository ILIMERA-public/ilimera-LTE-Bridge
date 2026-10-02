/*
 * İLİMERA LTE Bridge — 01 İlk GET İsteği (ESP32, kütüphanesiz)
 *
 * Köprünün {"evt":"ready",...} satırını bekler, ardından her 30 saniyede bir
 * HTTPS GET isteği gönderir ve gelen her satırı Seri Monitöre basar.
 * AT komutu, TCP/IP ya da TLS kütüphanesi gerekmez: köprü hepsini kendisi yapar.
 *
 * Bağlantı (köprünün J6 konnektörü — ESP32 DevKit örneği):
 *   Köprü TX → ESP32 GPIO16     Köprü RX ← ESP32 GPIO17     GND → GND
 *   115200 baud 8N1, 3.3 V lojik. Köprüyü USB-C ile besleyin.
 *
 * Her istek için iki satır gelir, hep bu sırayla:
 *   {"id":1,"acc":true}                       ← "aldım, kuyruğa koydum" (milisaniyeler içinde)
 *   {"id":1,"ok":true,"status":200,...}       ← gerçek sonuç (saniyeler sürebilir)
 *
 * EN: Waits for the bridge's ready event, then sends an HTTPS GET every 30 s and
 *     prints every line received. No AT, TCP/IP or TLS library needed.
 */

#include <Arduino.h>

// ─────────── KULLANICI AYARLARI ───────────
#define KOPRU_RX_PIN 16       // ESP32 RX ← köprü TX
#define KOPRU_TX_PIN 17       // ESP32 TX → köprü RX
#define ISTEK_URL    "https://httpbin.org/get"
const uint32_t ISTEK_ARALIGI_MS = 30000;
// ──────────────────────────────────────────

HardwareSerial kopru(2);
String satir;
bool kopruHazir = false;
uint32_t sonIstek = 0;
uint32_t istekNo = 0;

void satirGeldi(const String& s) {
  Serial.println("<< " + s);
  if (s.indexOf("\"evt\":\"ready\"") >= 0) {
    // Köprü (yeniden) başladı: o anda cevap beklenen istekler kaybolmuştur
    kopruHazir = true;
    sonIstek = 0;
  }
}

void setup() {
  Serial.begin(115200);
  kopru.begin(115200, SERIAL_8N1, KOPRU_RX_PIN, KOPRU_TX_PIN);
  delay(300);
  Serial.println("\n=== LTE Bridge ilk GET ===");
  Serial.println("Kopru hazir bildirimi bekleniyor...");
}

void loop() {
  // Köprüden gelen satırları oku (her cevap '\n' ile biter)
  while (kopru.available()) {
    char c = kopru.read();
    if (c == '\n') { satir.trim(); if (satir.length()) satirGeldi(satir); satir = ""; }
    else if (c != '\r' && satir.length() < 4600) satir += c;
  }

  // Köprü zaten açıksa "ready" satırını kaçırmış olabiliriz: 10 sn sonra yine de deneyelim
  if (!kopruHazir && millis() > 10000) kopruHazir = true;

  if (kopruHazir && (sonIstek == 0 || millis() - sonIstek >= ISTEK_ARALIGI_MS)) {
    sonIstek = millis();
    String istek = String("{\"cmd\":\"req\",\"id\":") + (++istekNo) + ",\"url\":\"" ISTEK_URL "\"}";
    Serial.println(">> " + istek);
    kopru.print(istek);
    kopru.print('\n');          // satır sonu şart: köprü isteği satır sonunda işler
  }
}
