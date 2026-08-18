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

Vereist: Qt6 (Core + SerialPort + Quick), CMake ≥ 3.16, een C++17-compiler,
en libpigpio (voor de TSic 506F-temperatuursensor, ZACwire via GPIO).

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

**pigpio — GEEN apt-pakket meer op Bookworm, éénmalig zelf bouwen vóór de
eerste `cmake --build`.** Fout hierboven overgeslagen? Op oudere Raspberry
Pi OS-versies (t/m Bullseye) leverde Raspberry Pi's eigen apt-repo een
kant-en-klaar `pigpio`-pakket met `pigpio.h` erbij — dat pakket bestaat niet
meer op Bookworm (er is geen `libpigpio-dev` op Raspberry Pi OS; dat is een
package-naam die op sommige andere Debian/Ubuntu-varianten iets heel anders
levert — de socket-client-headers voor de `pigpiod`-daemon, niet
`pigpio.h`). Eén keer zelf bouwen vanaf de officiële broncode volstaat:

```sh
sudo apt install -y git build-essential
git clone https://github.com/joan2937/pigpio.git
cd pigpio
git checkout v79
make
sudo make install
sudo ldconfig
cd ..
```

Dit zet `pigpio.h` in `/usr/local/include` en `libpigpio.so`/`.a` in
`/usr/local/lib` — standaard zoekpaden, dus `cmake --build build` vindt ze
daarna gewoon. Werkt ongewijzigd op de Pi 3B+ (BCM2837) — dit is puur een
Bookworm-pakketverdwijning, geen functionele breuk; **let op: pigpio werkt
sowieso NIET op een Pi 5** (andere GPIO-hardware/RP1-chip), maar dat is hier
niet van toepassing.

**Let op — pigpiod-daemon uitzetten indien geïnstalleerd.** `Tsic506Driver`
linkt rechtstreeks tegen libpigpio (geen `pigpiod`) en praat zelf met
`/dev/gpiomem`. Als de `pigpiod`-systemd-service ook draait, botsen de twee
op dezelfde GPIO-hardware: `sudo systemctl disable --now pigpiod` als die
per ongeluk actief staat.

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

**Let op — handmatig starten via een SSH-sessie geeft `Permission denied`.**
`./run.sh`/`gpsdo-monitor` rechtstreeks over SSH draaien loopt tegen twee
permissieproblemen aan die er bij een fysieke, lokale console-sessie niet
zijn: de seriële poorten (`Permission error while locking the device`,
normaal group `dialout`) en het HDMI-scherm zelf (`Could not set/queue DRM
mode/page flip ... Permission denied` — `systemd-logind` geeft eglfs/KMS-
toegang tot `/dev/dri/card0` dynamisch alleen aan de actieve lokale
("seat0") sessie, niet automatisch aan SSH). Zie "Automatisch starten bij
boot" hieronder voor de structurele oplossing — die omzeilt dit door de UI
als systemd-service te draaien i.p.v. handmatig vanuit een shell.

## Automatisch starten bij boot

Voor het toestel dat gewoon standalone moet draaien (touchscreen aan, geen
laptop/SSH nodig): `systemd/gpsdo-monitor.service` start `run.sh`
automatisch bij het opstarten, herstart 'm zelf bij een crash, en draait
als `root` — dat omzeilt in één keer zowel het seriële-poort- als het
DRM/seat-permissieprobleem hierboven (root heeft altijd toegang tot elk
device-node). `Environment=HOME=/home/wilfred` in de unit zorgt dat de
opgeslagen-Time-Mode-positie (`TimeModeSupervisor`, zie de projectbrief)
toch onder wilfreds eigen homedir blijft staan, ook al draait het proces
technisch als root.

```sh
sudo cp systemd/gpsdo-monitor.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now gpsdo-monitor.service
```

Status/logs bekijken:

```sh
systemctl status gpsdo-monitor
journalctl -u gpsdo-monitor -f
```

**Voor handmatig testen/debuggen** (bv. na een `git pull`+rebuild) moet de
service eerst gestopt worden — anders probeert een handmatige `./run.sh`
hetzelfde DRM-scherm te pakken dat de service al vasthoudt, en faalt:

```sh
sudo systemctl stop gpsdo-monitor
./run.sh                          # handmatig testen
sudo systemctl start gpsdo-monitor # weer aanzetten als je klaar bent
```

## Smoke-test zonder UI

```sh
./build/gpsdo_test_cli --fll /dev/ttyUSB0
./build/gpsdo_test_cli --gps /dev/ttyACM0
./build/gpsdo_test_cli --adc /dev/i2c-1
sudo ./build/gpsdo_test_cli --tsic 17   # 17 = Wilfreds gekozen GPIO voor de TSic 506F
./build/gpsdo_test_cli --fll /dev/ttyUSB0 --gps /dev/ttyACM0
```

Print live de FLL-statusregels, GPS-fix/satellietdata, ADC-spanningen en/of
TSic506-temperatuur naar stdout — handig om de bekabeling en drivers te
verifiëren los van de UI. Welke `/dev/tty*` bij welk apparaat hoort: de
VE2ZAZ (CH340-adapter) komt binnen als `/dev/ttyUSB0`, de u-blox LEA-5T
heeft een eigen USB-interface en komt binnen als `/dev/ttyACM0` — anders
even `dmesg | tail` na het inpluggen bekijken. `--tsic` heeft root nodig
(pigpio praat rechtstreeks met `/dev/gpiomem`).

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
- `src/Tsic506Driver.h` / `src/Tsic506Driver.cpp` — ZACwire-driver (via
  pigpio, GPIO-edge-interrupts) voor de IST AG TSic 506F-temperatuursensor,
  bedoeld om Δf/f-drift te correleren met omgevingstemperatuur bij de Rb-
  bron. Nog niet bekabeld/getest tegen een echte sensor.
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
