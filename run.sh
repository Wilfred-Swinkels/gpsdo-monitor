#!/bin/bash
# run.sh — "batch file"-achtige starter voor gpsdo_app op de Pi: regelt de
# juiste map + EGLFS-omgevingsvariabelen in één keer, zodat je niet meer
# elke keer handmatig `cd` + `QT_QPA_PLATFORM=eglfs ...` hoeft te typen.
#
# Werkt vanaf ELKE directory (ook via een symlink, zie hieronder) -- het
# script zoekt zijn EIGEN locatie op en cd't daarnaartoe, dus de relatieve
# ./build/gpsdo_app-aanroep klopt altijd, ongeacht vanwaar je het aanroept.
#
# Eenmalig installeren als los "commando" (op verzoek van Wilfred, i.p.v.
# steeds de volledige regel te moeten typen):
#   chmod +x run.sh
#   sudo ln -s "$(pwd)/run.sh" /usr/local/bin/gpsdo-monitor
# Daarna volstaat overal gewoon: gpsdo-monitor
#
# Extra argumenten worden doorgegeven aan gpsdo_app, bv.:
#   gpsdo-monitor --reset-survey-in

set -e

SCRIPT_DIR="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
cd "$SCRIPT_DIR"

export QT_QPA_PLATFORM=eglfs
export QT_QPA_EGLFS_HIDECURSOR=1

# --tsic 17: TSic 506F "Base plate"-temperatuursensor op BCM-GPIO17,
# toegevoegd 18-08-2026 nadat bleek dat de tegel/trendpagina leeg bleef
# ("—") omdat deze vlag hier nog ontbrak — de driver was al empirisch
# gevalideerd en aan GpsdoModel/QML gekoppeld, maar gpsdo_app kreeg de
# vlag simpelweg nooit doorgegeven. Vereist root, wat deze service toch al
# als zodanig draait (zie systemd/gpsdo-monitor.service).
#
# --adc /dev/i2c-1 --bite 27: MCP3426 (lamp-/xtal-spanning) + BITE-lock-
# detect, toegevoegd 18-08-2026 nadat de PCB/bekabeling klaar was. Zelfde
# les als bij --tsic hierboven: zonder deze vlaggen hier expliciet erbij
# te zetten, blijven die tegels/pagina's leeg, ook al zijn de drivers zelf
# klaar. LET OP: in tegenstelling tot --tsic zijn Mcp3426Adc en
# BiteLockDriver NOG NIET empirisch tegen echte hardware getest — als de
# lamp-/xtal-/RB LOCK-tegels na een herstart iets geks tonen (bv. een
# spanning die duidelijk niet klopt), eerst los verifiëren met
# `sudo systemctl stop gpsdo-monitor && sudo ./build/gpsdo_test_cli --adc
# /dev/i2c-1 --bite 27` vóór verder te zoeken in de UI-laag.
exec ./build/gpsdo_app --fll /dev/ttyUSB0 --gps /dev/ttyACM0 --tsic 17 \
    --adc /dev/i2c-1 --bite 27 "$@"
