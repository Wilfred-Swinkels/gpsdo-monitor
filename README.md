# GPSDO Monitor

GPS-disciplined rubidium (Efratom LPRO-101) 10MHz frequentiestandaard —
monitoring/logging-dashboard op een Raspberry Pi 3B+ met 720×720 touchscreen.
Puur uitlezen/visualiseren, geen sturing van de FLL-loop zelf.

Achtergrond, protocol-details, hardwarekeuzes en het UI-ontwerp staan in het
uitgebreide projectoverzicht (Claude Project "GPSDRBO").

## Status

Vroeg stadium: de kernlogica voor de seriële link naar de VE2ZAZ FLL-
controller, de UBX-link naar de u-blox LEA-5T GPS en de I2C-ADC-driver
staan er, plus een klein CLI-smoke-test-programma zonder UI. De echte
QML-app moet nog gebouwd worden.

## Bouwen (op de Raspberry Pi)

Vereist: Qt6 (Core + SerialPort), CMake ≥ 3.16, een C++17-compiler.

```sh
sudo apt install qt6-base-dev qt6-serialport-dev cmake build-essential
git clone https://github.com/Wilfred-Swinkels/gpsdo-monitor.git
cd gpsdo-monitor
cmake -B build
cmake --build build
```

## Smoke-test zonder UI

```sh
./build/gpsdo_test_cli --fll /dev/ttyUSB0
./build/gpsdo_test_cli --gps /dev/ttyUSB1
./build/gpsdo_test_cli --adc /dev/i2c-1
./build/gpsdo_test_cli --fll /dev/ttyUSB0 --gps /dev/ttyUSB1
```

Print live de FLL-statusregels, GPS-fix/satellietdata en/of ADC-spanningen
naar stdout — handig om de bekabeling en drivers te verifiëren voordat er
een UI omheen staat. Welke `/dev/ttyUSB*` bij welk apparaat hoort: even
uitproberen of `dmesg | tail` na het inpluggen bekijken.

## Snel itereren

```sh
git pull && cmake --build build && ./build/gpsdo_test_cli --fll /dev/ttyUSB0
```

## Bestanden

- `src/FllLink.h` / `src/FllLink.cpp` — seriële link + snel-lock-strategie
  (M02/S000A → M01/S0200) voor de VE2ZAZ GPS Std FLL-controller.
- `src/GpsLink.h` / `src/GpsLink.cpp` — UBX-link naar de u-blox LEA-5T:
  NAV-SVINFO (skyplot, per-satelliet CN0), NAV-SOL + NAV-DOP (fix-type,
  aantal satellieten, HDOP/PDOP).
- `src/Mcp3426Adc.h` / `src/Mcp3426Adc.cpp` — I2C-driver voor de MCP3426
  ADC (lamp-/xtal-spanning van de LPRO-101), met stale-data-detectie.
- `src/main_test_cli.cpp` — CLI-smoke-test-programma, nog geen UI.
