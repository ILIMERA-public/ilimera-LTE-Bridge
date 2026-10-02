# İLİMERA LTE Bridge — 4G Modem Arayüz Kartı

[English](README.en.md) · [Ürün sayfası](https://ilimera.com/urunler/gelistirme-kartlari/lte-bridge) · [Kullanıcı kılavuzu (PDF)](docs/ILIMERA_LTE_Bridge_kullanici_kilavuzu_v1.pdf) · [Protokol başvurusu](docs/PROTOKOL.md)

![LTE Bridge](docs/images/lte-bridge-main.webp)

**Cihazınızı tek bir AT komutu yazmadan internete çıkarın.** UART'tan tek satır JSON gönderirsiniz, tek satır
cevap okursunuz. SIM yönetimi, şebeke kaydı, APN, TCP yığını, TLS sertifikaları ve kopan bağlantıdan sonra
yeniden bağlanma kartın üzerinde olup biter. JSON dizgesi kurup seri porttan satır okuyabilen 8-bit bir
mikrodenetleyici bile HTTPS isteği atabilir.

```text
host → köprü   {"cmd":"req","id":1,"url":"https://api.ornek.com/v1/durum"}
köprü → host   {"id":1,"acc":true}
köprü → host   {"id":1,"ok":true,"status":200,"ctype":"application/json","body":"{\"state\":\"idle\"}"}
```

## İçindekiler

- [Özellikler](#özellikler)
- [Bağlantılar](#bağlantılar)
- [Hızlı başlangıç](#hızlı-başlangıç)
- [Host kodu yazarken dört kural](#host-kodu-yazarken-dört-kural)
- [Protokol başvurusu](docs/PROTOKOL.md)
- [Sorun giderme](#sorun-giderme)

## Özellikler

| Özellik | Değer |
| --- | --- |
| Hücresel modül | Cavli C16QS, 4G LTE |
| Host arayüzü | UART (J6): 3V3, GND, TX, RX — 115200 baud 8N1, 3.3 V lojik |
| Host protokolü | Satır sonuyla biten tek satır JSON istek / cevap |
| Desteklenen istekler | HTTP ve HTTPS: GET, POST, PUT, DELETE; kendi başlık ve gövdenizle |
| Güvenlik | Gömülü kök sertifika deposuyla doğrulanmış TLS, şifreli flash |
| Besleme ve servis | USB Type-C (besleme + servis konsolu) |
| SIM / anten | Nano SIM (bas-bırak), LTE anteni U.FL/IPEX |
| Güncelleme | Köprü ve modem için otomatik uzaktan güncelleme |
| Sınırlar | İstek satırı 2048 bayt, cevap gövdesi 4096 bayt, kuyruk 4 istek, zaman aşımı en çok 30 sn |

Planlananlar (uzaktan güncellemeyle gelecek): MQTT, ham TCP/UDP soketleri, TLS soketleri ve DTLS. Her istek bir
`"cmd"` alanı taşır; bugün bu kılavuza göre yazılan host kodu yeni komutlar geldiğinde değişmeden çalışır.

## Bağlantılar

![Kart üzerindeki bağlantılar](docs/images/lte-bridge-board.webp)

| Bağlantı | Ayrıntı |
| --- | --- |
| **Nano SIM yuvası** | Bas-bırak tipi. Kontaklar PCB'ye bakacak, kesik köşe yuvadaki işarete uyacak şekilde, enerji kapalıyken takın. APN'i genellikle şebeke verir |
| **LTE anteni** | LTE yazan U.FL/IPEX konnektör. Karta enerji vermeden önce takın |
| **Host UART (J6)** | 3V3 · GND · TX · RX. Kart TX → host RX, kart RX → host TX, GND → GND. 5 V host için seviye dönüştürücü gerekir |
| **USB Type-C** | Besleme ve servis konsolu (115200 baud sanal seri port). Konsol host hattından tamamen ayrıdır |

| Durum LED'i | Anlamı |
| --- | --- |
| Yanıp sönüyor (saniyede iki kez) | Henüz internet yok: açılıyor, yeniden bağlanıyor ya da kendini onarıyor |
| Sabit yanık | Bağlantı var, istekler karşılanabilir |

## Hızlı başlangıç

1. **SIM'i takın** (Nano, enerji kapalıyken).
2. **Anteni bağlayın**: U.FL fişini LTE konnektörüne "klik" sesi gelene kadar dik bastırın.
3. **Cihazınızı J6'ya bağlayın**: TX/RX çapraz, GND ortak, 115200 8N1, 3.3 V.
4. **USB-C ile enerji verin.** İsterseniz sanal seri portu 115200'de açıp açılış logunu izleyin.
5. `internet OK` satırını ve sabit yanan LED'i bekleyin. Host hattına şu satır düşer:
   `{"evt":"ready","fw":"0.2.6"}`. Artık istek gönderebilirsiniz.

## Host kodu yazarken dört kural

1. Her isteği **tek satır** olarak gönderin ve **`\n` ile bitirin**.
2. Bir sonraki isteği göndermeden önce öncekinin **sonuç satırını bekleyin** (aynı anda tek istek işlenir).
3. `ok:false` + `status` → sunucu reddetti, tekrar denemeyin. `status` yok → ulaşılamadı, biraz sonra tekrar deneyin.
4. Beklenmedik bir `{"evt":"ready"}` köprünün yeniden başladığını söyler: cevap beklediğiniz isteği yeniden gönderin.

## Sorun giderme

| Belirti | Nereye bakmalı |
| --- | --- |
| LED yanıp sönmeye devam ediyor | Veri bağlantısı yok: anten oturmuş mu, SIM takılı ve aktif mi, kapsama var mı? Servis konsolundaki log hangi adımda kalındığını söyler |
| USB seri portta hiçbir şey yok | Konsol açılıştan sonra bilerek sessizdir: Enter'a basın ya da `log` yazın |
| İstekler `err:"no_link"` dönüyor | Bağlantı koptu; kart zaten yeniden bağlanıyor, biraz sonra tekrar deneyin |
| İstekler `err:"no_time"` dönüyor | Saat henüz kurulmadı; bağlantıdan birkaç saniye sonra kendiliğinden geçer |
| İstekler `err:"busy"` dönüyor | Aynı anda çok istek gönderiyorsunuz; sonuç satırını bekleyin |
| Gönderdiğiniz satıra hiç cevap gelmiyor | Konsolda `host` yazın: `rx bytes` 0 ise kablo/lojik seviye sorunu; baytlar var ama `lines` yoksa `\n` eksik |
| Güncellemeden sonra kart takılmış görünüyor | 15 dakika enerjili bırakın; yeni yazılım internete çıkamazsa kart önceki sürüme kendiliğinden döner |

Tüm alanlar, hata kodları ve servis konsolu komutları: [docs/PROTOKOL.md](docs/PROTOKOL.md).

## Destek

Kullanıcı kılavuzu, güncellemeler ve destek için [ilimera.com](https://ilimera.com/urunler/gelistirme-kartlari/lte-bridge).
Bir hata bulduysanız ya da öneriniz varsa bu depoda **Issue** açabilirsiniz.

Modemi doğrudan AT komutlarıyla sürmek isterseniz aynı Cavli C16QS modülünü taşıyan
[CAVLI GSM/LTE Devboard](https://github.com/ILIMERA-public/CAVLI-GSMLTE-Devboard) deposuna bakın.

İLİMERA LTE Bridge, İLİMERA Teknoloji tarafından geliştirilmiştir.

## Lisans

Örnek kodlar [MIT lisansı](LICENSE) ile sunulur; kendi ürünlerinizde serbestçe kullanabilirsiniz.
Teknik dokümanlar ve görseller İLİMERA Teknoloji'ye aittir.
