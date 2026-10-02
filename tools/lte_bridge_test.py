#!/usr/bin/env python3
"""
İLİMERA LTE Bridge — bilgisayardan hızlı deneme

Köprünün J6 (host UART) konnektörünü bir USB-TTL dönüştürücüyle (3.3 V!)
bilgisayara bağlayıp istek göndermenizi sağlar. Mikrodenetleyici kodu
yazmadan önce SIM, anten ve sunucu adresinizi sınamak için kullanın.

Bağlantı:  köprü TX → dönüştürücü RX,  köprü RX → dönüştürücü TX,  GND → GND
Kurulum:   pip install pyserial
Kullanım:
    python lte_bridge_test.py COM5                         # https://httpbin.org/get
    python lte_bridge_test.py /dev/ttyUSB0 https://ornek.com/api
    python lte_bridge_test.py COM5 https://httpbin.org/post --post '{"sicaklik":23.5}'

Not: Bu port köprünün USB-C servis konsolu DEĞİL, J6 host hattıdır. Servis
konsolu (help, log, apn, host, ota) için USB-C portunu bir seri terminalle açın.

EN: Send requests to the bridge's J6 host UART from a PC through a 3.3 V USB-TTL
    adapter. Requires pyserial.
"""

import argparse
import json
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial gerekli:  pip install pyserial")


def main():
    p = argparse.ArgumentParser(description="LTE Bridge J6 hattına istek gönder")
    p.add_argument("port", help="Seri port (ör. COM5, /dev/ttyUSB0)")
    p.add_argument("url", nargs="?", default="https://httpbin.org/get")
    p.add_argument("--post", metavar="JSON", help="JSON gövdesiyle POST gönder")
    p.add_argument("--timeout", type=int, default=15000, help="istek zaman aşımı (ms, en çok 30000)")
    a = p.parse_args()

    istek = {"cmd": "req", "id": 1, "url": a.url, "timeout": a.timeout}
    if a.post:
        istek["method"] = "POST"
        istek["body"] = json.loads(a.post)

    with serial.Serial(a.port, 115200, timeout=0.5) as s:
        time.sleep(0.2)
        s.reset_input_buffer()
        satir = json.dumps(istek, ensure_ascii=False, separators=(",", ":"))
        print(">>", satir)
        s.write(satir.encode("utf-8") + b"\n")

        bitis = time.time() + a.timeout / 1000 + 10
        while time.time() < bitis:
            ham = s.readline()
            if not ham:
                continue
            metin = ham.decode("utf-8", "replace").strip()
            print("<<", metin)
            try:
                m = json.loads(metin)
            except ValueError:
                continue
            if m.get("id") != 1:
                continue
            if m.get("acc") is False:
                print("Istek kabul edilmedi:", m.get("err"))
                return 1
            if "ok" in m:
                if m["ok"]:
                    print("Basarili, HTTP", m.get("status"))
                    print(m.get("body", "")[:2000])
                    return 0
                if "status" in m:
                    print("Sunucu reddetti, HTTP", m["status"])
                else:
                    print("Sunucuya ulasilamadi:", m.get("err"))
                return 1
        print("Cevap gelmedi. Kabloyu (TX/RX capraz), 3.3 V seviyesini ve kopru LED'ini kontrol edin.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
