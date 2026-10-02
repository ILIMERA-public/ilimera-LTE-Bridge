/*
 * İLİMERA LTE Bridge — 03 8-bit Mikrodenetleyiciden HTTPS (Arduino Mega 2560)
 *
 * 8-bit bir denetleyicinin TLS kütüphanesi olmadan HTTPS isteği atabildiğini
 * gösterir. A0'daki analog değeri her 60 saniyede bir POST eder; JSON'u
 * snprintf ile kurar, cevaptaki "ok" ve "status" alanlarını basit metin
 * aramasıyla okur. Ek kütüphane gerekmez.
 *
 * Neden Mega?  Köprü 115200 baud kullanır; bu hız için donanımsal ikinci bir seri
 * port (Serial1) gerekir. Arduino Uno'da tek donanım seri portu USB'ye ayrılmıştır.
 *
 * Bağlantı — DİKKAT, Mega 5 V lojiktir, köprü 3.3 V:
 *   Mega TX1 (D18) → [seviye dönüştürücü] → köprü RX
 *   Mega RX1 (D19) ← [seviye dönüştürücü] ← köprü TX
 *   GND → GND.  Seviye dönüştürücü olmadan bağlamayın, köprü zarar görebilir.
 *
 * EN: Shows HTTPS from an 8-bit MCU (Arduino Mega, Serial1) without any TLS library.
 *     The Mega is 5 V: a level shifter is required on TX/RX.
 */

// ─────────── KULLANICI AYARLARI ───────────
#define POST_URL "https://httpbin.org/post"
const unsigned long OLCUM_ARALIGI_MS = 60000UL;
// ──────────────────────────────────────────

char satir[512];          // cevap gövdesinin tamamı gerekmiyor; ilk 511 bayt yeter
size_t uzunluk = 0;
unsigned long sonOlcum = 0;
unsigned int istekNo = 0;

void satirGeldi(const char* s) {
  Serial.print(F("<< ")); Serial.println(s);
  if (strstr(s, "\"evt\":\"ready\"")) { Serial.println(F("Kopru hazir.")); return; }
  if (strstr(s, "\"acc\":false"))     { Serial.println(F("Istek kabul edilmedi (kuyruk dolu?)")); return; }
  if (strstr(s, "\"ok\":true"))       { Serial.println(F("Gonderildi.")); return; }
  if (strstr(s, "\"ok\":false")) {
    if (strstr(s, "\"status\":")) Serial.println(F("Sunucu reddetti, tekrar denemeye gerek yok."));
    else                          Serial.println(F("Sunucuya ulasilamadi, sonra tekrar denenebilir."));
  }
}

void setup() {
  Serial.begin(115200);     // USB: Seri Monitör
  Serial1.begin(115200);    // köprü
  Serial.println(F("=== LTE Bridge, Arduino Mega ==="));
}

void loop() {
  while (Serial1.available()) {
    char c = Serial1.read();
    if (c == '\n') {
      satir[uzunluk] = '\0';
      if (uzunluk) satirGeldi(satir);
      uzunluk = 0;
    } else if (c != '\r' && uzunluk < sizeof(satir) - 1) {
      satir[uzunluk++] = c;   // sığmayan kısım atılır, satır sonu yine işlenir
    }
  }

  if (sonOlcum == 0 || millis() - sonOlcum >= OLCUM_ARALIGI_MS) {
    sonOlcum = millis();
    if (sonOlcum == 0) sonOlcum = 1;
    char istek[200];
    snprintf(istek, sizeof(istek),
             "{\"cmd\":\"req\",\"id\":%u,\"method\":\"POST\",\"url\":\"%s\","
             "\"body\":{\"a0\":%d,\"sure_s\":%lu}}",
             ++istekNo, POST_URL, analogRead(A0), millis() / 1000UL);
    Serial.print(F(">> ")); Serial.println(istek);
    Serial1.print(istek);
    Serial1.print('\n');
  }
}
