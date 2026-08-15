// main.cpp — echte QML-app (LCARS-stijl Overview-scherm, 720x720).
//
// Laadt QML rechtstreeks van schijf (QUrl::fromLocalFile), niet via het Qt
// Resource System — bewust, om twee redenen: (1) geen afhankelijkheid van
// qt_add_qml_module()/QML-cachegen, waarvan de exacte CMake-API per
// Qt6-minorversie verschilt en die ik hier niet tegen de echte Pi-Qt-versie
// kan verifiëren; (2) hierdoor kunnen QML-only wijzigingen op de Pi getest
// worden door alleen te 'git pull'-en, zonder te hoeven herbouwen — sluit
// aan bij de "snel itereren"-werkwijze van dit hele project.
//
// Gebruik:
//   gpsdo_app --fll /dev/ttyUSB0 --gps /dev/ttyACM0

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QUrl>
#include <QTextStream>
#include <QDebug>

#include "FllLink.h"
#include "GpsLink.h"
#include "GpsdoModel.h"

#ifndef QML_DIR
#error "QML_DIR moet gedefinieerd zijn door CMakeLists.txt (pad naar de qml/-map in de broncode)"
#endif

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("gpsdo_app"));

    QCommandLineParser parser;
    parser.setApplicationDescription("GPSDO monitor - LCARS-stijl UI");
    parser.addHelpOption();

    QCommandLineOption fllOpt("fll", "Seriele poort naar de VE2ZAZ FLL-controller (bv. /dev/ttyUSB0)", "device");
    QCommandLineOption gpsOpt("gps", "Seriele poort naar de u-blox LEA-5T (bv. /dev/ttyACM0)", "device");
    parser.addOption(fllOpt);
    parser.addOption(gpsOpt);
    parser.process(app);

    FllLink fllLink;
    GpsLink gpsLink;
    GpsdoModel gpsdoModel;
    gpsdoModel.attachFllLink(&fllLink);
    gpsdoModel.attachGpsLink(&gpsLink);

    if (parser.isSet(fllOpt)) {
        if (!fllLink.open(parser.value(fllOpt)))
            QTextStream(stderr) << "Waarschuwing: kon FLL-seriele poort niet openen: " << parser.value(fllOpt) << "\n";
    } else {
        QTextStream(stderr) << "Waarschuwing: geen --fll opgegeven, FLL-tegel blijft leeg.\n";
    }

    if (parser.isSet(gpsOpt)) {
        if (!gpsLink.open(parser.value(gpsOpt)))
            QTextStream(stderr) << "Waarschuwing: kon GPS-seriele poort niet openen: " << parser.value(gpsOpt) << "\n";
    } else {
        QTextStream(stderr) << "Waarschuwing: geen --gps opgegeven, GPS-tegel blijft leeg.\n";
    }

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("gpsdoModel"), &gpsdoModel);

    const QString qmlDir = QStringLiteral(QML_DIR);
    const QUrl mainQmlUrl = QUrl::fromLocalFile(qmlDir + QStringLiteral("/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                      &app, []() { QCoreApplication::exit(-1); }, Qt::QueuedConnection);

    engine.load(mainQmlUrl);
    if (engine.rootObjects().isEmpty()) {
        QTextStream(stderr) << "Kon QML niet laden vanaf " << mainQmlUrl.toString() << "\n";
        return -1;
    }

    return app.exec();
}
