# LTE Bridge Host Protokolü (firmware v0.2.6)

Cihazınız köprüyle J6 konnektörü üzerinden konuşur: **115200 baud, 8N1, 3.3 V lojik**. Her istek ve her cevap
**satır sonuyla (`\n`) biten tek satır JSON**'dur. Host hattına bunun dışında hiçbir şey yazılmaz; teşhis
çıktıları yalnız USB-C servis konsoluna gider.

Ayrıntılı anlatım: [kullanıcı kılavuzu (PDF)](ILIMERA_LTE_Bridge_kullanici_kilavuzu_v1.pdf).

## Olaylar (köprü → host)

| Satır | Anlamı |
| --- | --- |
| `{"evt":"ready","fw":"0.2.6"}` | Protokol hazır (internet henüz olmayabilir). Beklenmedik anda gelirse köprü yeniden başlamıştır; cevap beklenen istekler kaybolmuştur, yeniden gönderin |
| `{"evt":"fuota","target":"modem","phase":"start","from":"1.5.8","to":"1.6.0"}` | Modem yazılımı güncelleniyor; birkaç dakika istek karşılanmaz |
| `{"evt":"fuota","target":"modem","phase":"done",...}` | Güncelleme bitti, istek gönderebilirsiniz |

## İstek (host → köprü)

```json
{"cmd":"req","id":1,"url":"https://api.ornek.com/v1/durum"}
```

```json
{"cmd":"req","id":2,"method":"POST","url":"https://api.ornek.com/v1/veri","headers":{"Authorization":"Bearer abc123"},"body":{"sicaklik":23.5,"nem":41},"timeout":15000}
```

| Alan | Zorunlu | Anlamı |
| --- | --- | --- |
| `cmd` | evet | Bugün her zaman `"req"`. Yeni protokoller (MQTT, TCP/UDP) yeni değerler olarak gelecek |
| `url` | evet | Tam adres. `https://` doğrulanmış TLS kullanır, `http://` şifresizdir |
| `id` | hayır | Sizin eşleme numaranız; bu isteğe ait her satırda aynen geri döner |
| `method` | hayır | `GET`, `POST`, `PUT`, `DELETE`. Gövde varsa varsayılan `POST`, yoksa `GET` |
| `headers` | hayır | Ek başlıklar; değerler string olmalı |
| `body` | hayır | Nesne/dizi JSON olarak gönderilir; string verilirse baytlar aynen gider (form verisi, XML) |
| `timeout` | hayır | Milisaniye; varsayılan 15000, en çok 30000 |

## Cevap (köprü → host) — her zaman iki satır, hep bu sırayla

```json
{"id":1,"acc":true}
{"id":1,"ok":true,"status":200,"ctype":"application/json","body":"{\"state\":\"idle\"}"}
```

1. **`acc` satırı** milisaniyeler içinde gelir: "aldım, kuyruğa koydum". `acc:false` ise istek reddedilmiştir ve
   ikinci satır **gelmez**.
2. **Sonuç satırı** saniyeler sürebilir.

| Alan | Anlamı |
| --- | --- |
| `ok` | Yalnız HTTP 2xx için `true` |
| `status` | HTTP durum kodu. **Yoksa sunucuya hiç ulaşılamamıştır** |
| `ctype` | Sunucunun Content-Type'ı (parametreleri atılmış) |
| `body` | Her zaman string; köprü baytları ayrıştırmadan geçirir |
| `trunc` | Cevap 4096 baytı aştı ve kesildi (yalnız `true` iken bulunur) |
| `err` | İstek köprü tarafında başarısız olduysa sebebi (aşağıda) |

## Hata kodları ve yeniden deneme

| `err` | Anlamı | Tekrar denenir mi |
| --- | --- | --- |
| `parse` | Gelen satır geçerli JSON değil | Hayır |
| `toolong` | Satır 2048 baytı aştı | Hayır |
| `cfg` | İstek geçersiz (url yok, metot tanınmıyor…) | Hayır |
| `busy` | Kuyruk dolu (4 bekleyen istek) | Evet, biraz sonra |
| `no_link` | Şu an mobil bağlantı yok | Evet |
| `no_time` | Saat kurulmadı, TLS başlatılamıyor | Evet |
| `dns` | Alan adı çözümlenemedi | Evet |
| `conn` | Bağlantı kurulamadı | Evet |
| `timeout` | Sunucu süresinde cevap vermedi | Evet |
| `mem` | Bellek yetmedi | Evet |

**Kural:** `ok:false` ve yanında `status` varsa sunucu cevap verip reddetmiştir, tekrar denemek işe yaramaz.
`status` yoksa sunucuya ulaşılamamıştır, tekrar denemek genelde işe yarar. Bu kuralın uygulanmış hâli:
[`02_Sensor_Verisi_POST`](../examples/02_Sensor_Verisi_POST).

## Sınırlar

| Sınır | Değer | Aşılınca |
| --- | --- | --- |
| Aynı anda uçuşta istek | 1 | Diğerleri kuyrukta bekler |
| Kuyruk derinliği | 4 | `acc:false`, `err:"busy"` |
| Satır uzunluğu | 2048 bayt | `err:"toolong"`, satır atılır |
| Cevap gövdesi | 4096 bayt | Kesilir, `trunc:true` |
| Zaman aşımı tavanı | 30 sn | Büyük değerler tavana kırpılır |

## Servis konsolu (USB-C, 115200 baud)

| Komut | Ne yapar |
| --- | --- |
| `help` | Komutları listeler |
| `log` / `log clear` | Açılıştan beri tutulan logu basar / boşaltır |
| `debug`, `debug 0` · `debug 1` | Log seviyesi: 0 = yalnız kilit adımlar (varsayılan), 1 = ayrıntılı |
| `apn`, `apn <ad>`, `apn clear` | Kullanılan APN'i göster / elle belirle / şebekenin verdiğine dön |
| `host` | Host UART sayaçları: gelen bayt, ayrıştırılan satır, gönderilen satır |
| `modem` | Modem yazılım sürümü ve güncelleme geçmişi |
| `ota`, `ota 0` · `ota 1` | Uzaktan güncelleme durumu / kapat / aç (kalıcı) |
| `reset` | Köprüyü yeniden başlatır |

Beklenen açılış logu:

```text
====== LTE-Bridge Firmware ======
[BOOT] Firmware: 0.2.6
[BOOT] Host  UART0  TX=3 RX=4 @115200
modem ready
connecting to network...
registered on network, PDP context activated
IP 10.60.85.79 (APN: m2mgenel)
internet OK
[BOOT] Kurulum tamam
[OTA] Guncel (0.2.6)
[STAT] ppp=UP cmux=open heap=203560 modem_rst=0
```

`host` komutunda `rx bytes` sıfırda kalıyorsa sorun kabloda ya da lojik seviyededir; baytlar artıyor ama `lines`
artmıyorsa satır sonu (`\n`) gönderilmiyordur.

> **`ota 0` için iki kez düşünün:** Üretim kartında uzaktan güncelleme içeri giden tek uzak yoldur. Güncellemesi
> kapatılmış bir kart yalnız fiziksel erişimle kurtarılabilir.
