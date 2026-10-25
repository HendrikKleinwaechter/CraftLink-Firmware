# Fremdkomponenten (CraftLink-Firmware)

Der CraftLink-eigene Code steht unter der **MIT-Lizenz** (`LICENSE`). Die fertigen `.uf2`-Dateien in `fertig/` sind mit
den folgenden Komponenten gebaut und enthalten Teile davon. Für diese Teile gelten ihre eigenen Lizenzen; die
vollständigen Texte liegen im Ordner `LICENSES/`.

Gebaut mit **arduino-pico 5.6.0** (Earle F. Philhower III) und Einstellung „USB Stack: Adafruit TinyUSB“.

| Komponente | Version | Lizenz | Text | in welchen .uf2 |
|---|---|---|---|---|
| arduino-pico (Arduino-Core, Kern-Bibliotheken) | 5.6.0 | LGPL 2.1 | `LICENSES/LGPL-2.1_arduino-pico.txt` | alle |
| Raspberry Pi Pico SDK | 2.2.1 (in arduino-pico 5.6.0) | BSD-3-Clause, Copyright 2020 Raspberry Pi (Trading) Ltd. | `LICENSES/BSD-3-Clause_pico-sdk.txt` | alle |
| TinyUSB | 0.18.0 (im Pico SDK) | MIT, Copyright (c) 2018 hathach (tinyusb.org) | `LICENSES/MIT_tinyusb.txt` | alle |
| Adafruit TinyUSB Library | 3.7.4 | MIT, Copyright (c) 2019 Ha Thach for Adafruit Industries | `LICENSES/MIT_Adafruit-TinyUSB.txt` | alle |
| cyw43-driver (WLAN-Chip, über den die LED am Pico W läuft) mit der Firmware des WLAN-Chips | im Pico SDK 2.2.1 | Lizenz für die Nutzung mit Chips von Raspberry Pi Ltd, Copyright (C) 2019-2022 George Robotics Pty Ltd | `LICENSES/cyw43-driver_LICENSE.RP.txt` | nur Pico W und Pico 2 W |

## LGPL-Hinweis (arduino-pico)

Die fertigen `.uf2`-Dateien enthalten LGPL-lizenzierte Teile des Arduino-Cores. Die LGPL erlaubt das, wenn du diese Teile
durch eigene Fassungen ersetzen und das Ganze neu bauen kannst. Das geht: Der vollständige Quellcode der CraftLink-Firmware
liegt hier (MIT, Änderungen und Fehlersuche für jeden Zweck erlaubt), gebaut wird mit `arduino-cli` bzw. der Arduino IDE
(Anleitung in `README.md`).

Den Quellcode von arduino-pico in der verwendeten Version 5.6.0 bekommst du unter
<https://github.com/earlephilhower/arduino-pico/releases/tag/5.6.0>. Zusätzlich gilt: **Auf Anfrage schicke ich dir den
Quellcode der LGPL-Bestandteile in genau der verwendeten Version** – drei Jahre lang ab Veröffentlichung der jeweiligen
`.uf2`, gegen höchstens die Kosten für den Versand (Kontakt: kontakt@hendriks-werkstatt.de).

## cyw43-driver (nur Pico W / Pico 2 W)

Der Treiber und die Firmware des WLAN-Chips dürfen nur zusammen mit Chips von Raspberry Pi Ltd (RP2040, RP2350)
verwendet und weitergegeben werden. Die CraftLink-Firmware läuft nur auf diesen Chips. Wer sie auf andere Hardware
übertragen will, muss die Pico-W-Variante weglassen oder sich eine eigene Lizenz von George Robotics besorgen.

> Stand 23.10.2026. Vor jeder neuen `.uf2` mit den tatsächlich verwendeten Versionen abgleichen.
