# GPSDO Monitor

GPS-disciplined rubidium (Efratom LPRO-101) 10MHz frequentiestandaard —
monitoring/logging-dashboard op een Raspberry Pi 3B+ met 720×720 touchscreen.
Puur uitlezen/visualiseren, geen sturing van de FLL-loop zelf.

Achtergrond, protocol-details, hardwarekeuzes en het UI-ontwerp staan in het
uitgebreide projectoverzicht (Claude Project "GPSDRBO").

## Status

De kernlogica voor de seriële link naar de VE2ZAZ FLL-controller en de
UBX-link naar de u-blox LEA-5T GPS zijn tegen echte hardware geverifieerd.
Er is nu ook een eerste echt QML-scherm (Overview-pagina, LCARS-stijl,
720×720) bovenop die twee links. De I2C-ADC-driver (MCP3426) staat er
klaar maar is nog niet bekabeld — Wilfred maakt nog de PCB — dus de
lamp-/xtal-spanning op het scherm is voorlopig een placeholder ("—").

## Bouwen (op de Raspberry Pi)

Vereist: Qt6 (Core + SerialPort + Quick), CMake ≥ 3.16, een C++17-compiler.

```sh
sudo apt install qt6-base-dev qt6-serialport-dev qt6-declarative-dev cmake build-essential
git clone https://github.com/Wilfred-Swinkels/gpsdo-monitor.git
cd gpsdo-monitor
cmake -B build
cmake --build build
```

Als `qt6-declarative-dev` niet bestaat onder die naam op jouw Raspberry Pi OS-
versie: zoek met `apt search qt6 | grep -i quick` naar het juiste pakket
(vaak iets als `qt6-quick-dev` of `qt6-declarative`).

## De echte UI draaien

Op de eigenlijke Pi-console (KMS/DRM, geen X/Wayland) draait `gpsdo_app` via
het `eglfs`-platform-plugin, met de cursor uitgezet:

```sh
QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_HIDECURSOR=1 ./build/gpsdo_app --fll /dev/ttyUSB0 --gps /dev/ttyACM0
```

Opent een 720×720 venster met de Overview-pagina: lock-status (LOCKED/
UNLOCKED/HOLDOVER/DISABLED, kleurcodering zoals het LCARS-ontwerp),
Δf/f-nauwkeurigheid, DAC-waarde, en een 2×3 grid met GPS-kerngetallen
(sats gebruikt/zichtbaar, gemiddelde SNR, fix-type, HDOP) plus de FLL-
status. `--fll` en `--gps` zijn allebei optioneel — laat je er een weg,
dan blijft het bijbehorende deel van het scherm op "—" staan i.p.v. dat
het programma crasht.

**Kortere starter — `run.sh`** regelt bovenstaande `cd`+env-vars+commando in
één keer (net als een DOS-batchbestand):

```sh
./run.sh
```

Eenmalig instellen als los systeemcommando, zodat je overal gewoon
`gpsdo-monitor` kunt typen (ook buiten deze map):

```sh
chmod +x run.sh
sudo ln -s "$(pwd)/run.sh" /usr/local/bin/gpsdo-monitor
```

Daarna start `gpsdo-monitor` de app vanaf elke directory — het script zoekt
zelf zijn locatie op en `cd`'t daarnaartoe, dus dit hoeft maar één keer.
Extra argumenten worden doorgegeven, bv. `gpsdo-monitor --reset-survey-in`.

De QML-bestanden (`qml/*.qml`) worden rechtstreeks van schijf geladen, dus
een QML-only wijziging testen op de Pi is gewoon `git pull` — geen
herbouwen nodig, alleen `gpsdo_app` (of `gpsdo-monitor`) opnieuw starten.

## Smoke-test zonder UI

```sh
./build/gpsdo_test_cli --fll /dev/ttyUSB0
./build/gpsdo_test_cli --gps /dev/ttyACM0
./build/gpsdo_test_cli --adc /dev/i2c-1
./build/gpsdo_test_cli --fll /dev/ttyUSB0 --gps /dev/ttyACM0
```

Print live de FLL-statusregels, GPS-fix/satellietdata en/of ADC-spanningen
naar stdout — handig om de bekabeling en drivers te verifiëren los van de
UI. Welke `/dev/tty*` bij welk apparaat hoort: de VE2ZAZ (CH340-adapter)
komt binnen als `/dev/ttyUSB0`, de u-blox LEA-5T heeft een eigen USB-
interface en komt binnen als `/dev/ttyACM0` — anders even `dmesg | tail`
na het inpluggen bekijken.

## Snel itereren

```sh
git pull && cmake --build build && ./run.sh
```

## Bestanden

- `src/FllLink.h` / `src/FllLink.cpp` — seriële link + snel-lock-strategie
  (M02/S000A → M01/S0200) voor de VE2ZAZ GPS Std FLL-controller.
- `src/GpsLink.h` / `src/GpsLink.cpp` — UBX-link naar de u-blox LEA-5T:
  NAV-SVINFO (skyplot, per-satelliet CN0), NAV-SOL + NAV-DOP (fix-type,
  aantal satellieten, HDOP/PDOP).
- `src/Mcp3426Adc.h` / `src/Mcp3426Adc.cpp` — I2C-driver voor de MCP3426
  ADC (lamp-/xtal-spanning van de LPRO-101), met stale-data-detectie. Nog
  niet bekabeld.
- `src/main_test_cli.cpp` — CLI-smoke-test-programma, geen UI.
- `app/GpsdoModel.h` / `app/GpsdoModel.cpp` — vertaallaag tussen FllLink/
  GpsLink en de QML-UI (properties als `lockLabel`, `accuracyText`,
  `satsUsed`, ...).
- `app/main.cpp` — QGuiApplication + QQmlApplicationEngine, opent de
  seriële links en laadt `qml/main.qml`.
- `qml/main.qml` — LCARS-kaderwerk (header met klok, linkerrail, footer)
  rond de pagina's.
- `qml/OverviewPage.qml` — de Overview-pagina: lock-hero, accuracy-strip,
  lamp-/xtal-placeholder, 2×3 stat-grid.
- `qml/StatTile.qml` — herbruikbare label+waarde-tegel.
