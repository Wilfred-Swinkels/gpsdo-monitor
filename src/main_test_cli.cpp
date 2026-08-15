// main_test_cli.cpp
//
// Klein smoke-test-programma zonder UI: opent FllLink en/of Mcp3426Adc en
// print alles wat binnenkomt naar stdout. Bedoeld om de seriële link naar
// de VE2ZAZ en/of de I2C-ADC te verifiëren zodra de hardware fysiek
// aangesloten is — ruim vóór de echte QML-app er staat.
//
// Gebruik:
//   gpsdo_test_cli --fll /dev/ttyUSB0
//   gpsdo_test_cli --adc /dev/i2c-1 [--adc-addr 0x68]
//   gpsdo_test_cli --fll /dev/ttyUSB0 --adc /dev/i2c-1

#include <QCoreApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QTextStream>
#include <QDebug>

#include "FllLink.h"
#include "Mcp3426Adc.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("GPSDO monitor - smoke test CLI (geen UI)");
    parser.addHelpOption();

    QCommandLineOption fllOpt("fll", "Seriele poort naar de VE2ZAZ FLL-controller (bv. /dev/ttyUSB0)", "device");
    QCommandLineOption adcOpt("adc", "I2C-device voor de MCP3426 (bv. /dev/i2c-1)", "device");
    QCommandLineOption adcAddrOpt("adc-addr", "I2C-adres van de MCP3426 in hex (default 0x68 = suffix A0)", "addr", "0x68");
    parser.addOption(fllOpt);
    parser.addOption(adcOpt);
    parser.addOption(adcAddrOpt);
    parser.process(app);

    if (!parser.isSet(fllOpt) && !parser.isSet(adcOpt)) {
        QTextStream(stderr) << "Geef minstens --fll of --adc op. Zie --help.\n";
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

        if (!fllLink.open(parser.value(fllOpt))) {
            QTextStream(stderr) << "Kon FLL-seriele poort niet openen.\n";
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

    return app.exec();
}
