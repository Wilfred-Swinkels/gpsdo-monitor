// main_test_cli.cpp
//
// Klein smoke-test-programma zonder UI: opent FllLink, GpsLink, Mcp3426Adc
// en/of Tsic506Driver en print alles wat binnenkomt naar stdout. Bedoeld om
// de seriële links, de I2C-ADC en de ZACwire-temperatuursensor te
// verifiëren zodra de hardware fysiek aangesloten is — ruim vóór de echte
// QML-app er staat.
//
// Gebruik:
//   gpsdo_test_cli --fll /dev/ttyUSB0
//   gpsdo_test_cli --gps /dev/ttyUSB1
//   gpsdo_test_cli --adc /dev/i2c-1 [--adc-addr 0x68]
//   gpsdo_test_cli --tsic 17       (BCM-GPIO-nummer, niet de pinheader-positie —
//                                    17 is Wilfreds gekozen pin voor de TSic 506F,
//                                    zie Tsic506Driver.h)
//   gpsdo_test_cli --fll /dev/ttyUSB0 --gps /dev/ttyUSB1
//
// --tsic vereist root (pigpio praat rechtstreeks met /dev/gpiomem), dus:
//   sudo ./build/gpsdo_test_cli --tsic 17
//
// De MCP3426 (--adc) hangt aan de standaard I2C1-bus van de Pi (GPIO2=SDA/
// GPIO3=SCL, /dev/i2c-1) — geen apart GPIO-nummer nodig, dat ligt al vast
// aan het device-pad.

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QTextStream>
#include <QDebug>
#include <QStringList>

#include "FllLink.h"
#include "Mcp3426Adc.h"
#include "GpsLink.h"
#include "Tsic506Driver.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("GPSDO monitor - smoke test CLI (geen UI)");
    parser.addHelpOption();

    QCommandLineOption fllOpt("fll", "Seriele poort naar de VE2ZAZ FLL-controller (bv. /dev/ttyUSB0)", "device");
    QCommandLineOption gpsOpt("gps", "Seriele poort naar de u-blox LEA-5T (bv. /dev/ttyUSB1)", "device");
    QCommandLineOption adcOpt("adc", "I2C-device voor de MCP3426 (bv. /dev/i2c-1)", "device");
    QCommandLineOption adcAddrOpt("adc-addr", "I2C-adres van de MCP3426 in hex (default 0x68 = suffix A0)", "addr", "0x68");
    QCommandLineOption tsicOpt("tsic", "BCM-GPIO-nummer van de TSic 506F ZACwire-signaalpin (Wilfreds gekozen pin: 17)", "gpio", "17");
    parser.addOption(fllOpt);
    parser.addOption(gpsOpt);
    parser.addOption(adcOpt);
    parser.addOption(adcAddrOpt);
    parser.addOption(tsicOpt);
    parser.process(app);

    if (!parser.isSet(fllOpt) && !parser.isSet(gpsOpt) && !parser.isSet(adcOpt) && !parser.isSet(tsicOpt)) {
        QTextStream(stderr) << "Geef minstens --fll, --gps, --adc of --tsic op. Zie --help.\n";
        return 1;
    }

    FllLink fllLink;
    if (parser.isSet(fllOpt)) {
        QObject::connect(&fllLink, &FllLink::statusUpdated, [](const FllStatus &s) {
            qInfo().noquote() << QStringLiteral("FLL state=%1 dac=0x%2 freq=0x%3 accum=%4 sampleCnt=%5")
                                      .arg(s.state)
                                      .arg(s.dacValue, 4, 16, QLatin1Char('0'))
                                      .arg(s.freqReadout, 4, 16, QLatin1Char('0'))
                                      .arg(s.accumFreqDiff)
                                      .arg(s.sampleCounter);
        });
        QObject::connect(&fllLink, &FllLink::lockAcquired, []() { qInfo() << "*** LOCK ACQUIRED ***"; });
        QObject::connect(&fllLink, &FllLink::lockLost, []() { qInfo() << "*** LOCK LOST ***"; });
        QObject::connect(&fllLink, &FllLink::rawLineReceived, [](const QByteArray &line) {
            qInfo().noquote() << "FLL raw:" << line;
        });
        QObject::connect(&fllLink, &FllLink::parseError, [](const QByteArray &line, const QString &reason) {
            qWarning().noquote() << QStringLiteral("FLL parse-fout (%1): %2").arg(reason, QString::fromLatin1(line));
        });

        if (!fllLink.open(parser.value(fllOpt))) {
            QTextStream(stderr) << "Kon FLL-seriele poort niet openen.\n";
            return 1;
        }
    }

    GpsLink gpsLink;
    if (parser.isSet(gpsOpt)) {
        QObject::connect(&gpsLink, &GpsLink::fixUpdated, [](const GpsFix &f) {
            qInfo().noquote() << QStringLiteral("GPS fix=%1 ok=%2 sats=%3 hdop=%4 pdop=%5")
                                      .arg(f.fixType)
                                      .arg(f.fixOk)
                                      .arg(f.numSatellites)
                                      .arg(f.hdop, 0, 'f', 2)
                                      .arg(f.pdop, 0, 'f', 2);
        });
        QObject::connect(&gpsLink, &GpsLink::satellitesUpdated, [](const QList<GpsSatellite> &sats) {
            int used = 0;
            for (const auto &s : sats)
                if (s.usedInFix)
                    ++used;
            qInfo().noquote() << QStringLiteral("GPS sats: %1 zichtbaar, %2 gebruikt in fix")
                                      .arg(sats.size())
                                      .arg(used);
            for (const auto &s : sats) {
                qInfo().noquote() << QStringLiteral("  sv%1: elev=%2 azim=%3 cno=%4dBHz used=%5%6")
                                          .arg(s.svid)
                                          .arg(s.elevationDeg)
                                          .arg(s.azimuthDeg)
                                          .arg(s.cno)
                                          .arg(s.usedInFix)
                                          .arg(s.unhealthy ? " UNHEALTHY" : "");
            }
        });
        QObject::connect(&gpsLink, &GpsLink::errorOccurred, [](const QString &msg) {
            qWarning().noquote() << "GPS-fout:" << msg;
        });

        if (!gpsLink.open(parser.value(gpsOpt))) {
            QTextStream(stderr) << "Kon GPS-seriele poort niet openen.\n";
            return 1;
        }
    }

    Mcp3426Adc adc;
    if (parser.isSet(adcOpt)) {
        bool ok = false;
        quint8 addr = static_cast<quint8>(parser.value(adcAddrOpt).toUShort(&ok, 16));
        if (!ok)
            addr = 0x68;

        QObject::connect(&adc, &Mcp3426Adc::voltageRead, [](const QString &name, double v, qint16 raw) {
            qInfo().noquote() << QStringLiteral("ADC %1: %2 V (raw=%3)")
                                      .arg(name)
                                      .arg(v, 0, 'f', 4)
                                      .arg(raw);
        });
        QObject::connect(&adc, &Mcp3426Adc::errorOccurred, [](const QString &msg) {
            qWarning().noquote() << "ADC-fout:" << msg;
        });

        // TODO: dividerRatio is nog 1.0 (geen deler) — pas aan zodra de
        // spanningsdelers gebouwd en empirisch gekalibreerd zijn tegen de
        // echte J1-pinnen 5 (lamp) en 9 (xtal). Zie Mcp3426Adc.h.
        Mcp3426Adc::ChannelConfig ch1;
        ch1.channel = 1;
        ch1.name = QStringLiteral("lamp");
        ch1.dividerRatio = 1.0;

        Mcp3426Adc::ChannelConfig ch2;
        ch2.channel = 2;
        ch2.name = QStringLiteral("xtal");
        ch2.dividerRatio = 1.0;

        adc.start(parser.value(adcOpt), addr, ch1, ch2);
    }

    Tsic506Driver tsic;
    if (parser.isSet(tsicOpt)) {
        bool ok = false;
        const int gpio = parser.value(tsicOpt).toInt(&ok);
        if (!ok) {
            QTextStream(stderr) << "Ongeldig --tsic GPIO-nummer.\n";
            return 1;
        }

        QObject::connect(&tsic, &Tsic506Driver::temperatureRead, [](double celsius, quint16 raw) {
            qInfo().noquote() << QStringLiteral("TSic506: %1 °C (raw=%2)")
                                      .arg(celsius, 0, 'f', 3)
                                      .arg(raw);
        });
        QObject::connect(&tsic, &Tsic506Driver::errorOccurred, [](const QString &msg) {
            qWarning().noquote() << "TSic506-fout:" << msg;
        });
        // Diagnose (18-08-2026): rauwe Tstrobe + per-bit lage-fase-duren
        // voor ELK gedecodeerd byte (geslaagd én mislukt), om de resterende
        // ZACwire-decodeerfouten te kunnen onderzoeken — zie Tsic506Driver.h.
        QObject::connect(&tsic, &Tsic506Driver::byteDecoded,
                          [](quint32 strobeUs, QVector<quint32> bitDurationsUs, quint8 byteValue, bool parityOk) {
            QStringList durs;
            for (quint32 d : bitDurationsUs)
                durs << QString::number(d);
            qDebug().noquote() << QStringLiteral("TSic506-debug: Tstrobe=%1us byte=0b%2 pariteitOK=%3 duren=[%4]")
                                       .arg(strobeUs)
                                       .arg(static_cast<int>(byteValue), 8, 2, QChar('0'))
                                       .arg(parityOk ? "ja" : "NEE")
                                       .arg(durs.join(","));
        });

        tsic.start(gpio);
    }

    return app.exec();
}
