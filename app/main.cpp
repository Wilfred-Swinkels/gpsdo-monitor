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
//
// GPS Time Mode Survey-In (--start-survey-in): bepaalt de vaste antenne-
// positie door minuten tot dagen te middelen, en de LEA-5T schakelt daarna
// zelf over naar Fixed Mode (zie GpsLink.h). Deze flag is IDEMPOTENT/veilig
// om standaard in het opstartcommando te laten staan — bij elke app-start
// wordt eerst de HUIDIGE Time Mode-status bij de module opgevraagd
// (GpsLink::requestAutoSurveyIn()), en alleen als die nog op Disabled staat
// wordt Survey-In daadwerkelijk gestart. Een module die al aan het surveyen
// is, of al Fixed staat, wordt met rust gelaten — een simpele app-herstart
// (zonder dat de USB-module zelf stroomloos is geweest) reset dus niets.
//
// Antenne verplaatst? Dat kan deze hardware niet zelf detecteren zodra hij
// eenmaal Fixed staat (zie GpsLink.h) — gebruik dan expliciet
// --reset-survey-in om een verse meting te forceren, OF zet --verify-position
// aan om dit automatisch bij elke opstart te laten controleren (zie
// TimeModeSupervisor.h). Dat laatste is bewust een aparte, opt-in vlag: het
// kost een korte Time Mode-onderbreking bij ELKE herstart terwijl de module
// al Fixed staat (typisch enkele tientallen seconden), wat --start-survey-in
// hierboven expres niet doet.

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
#include "TimeModeSupervisor.h"

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
    QCommandLineOption surveyInOpt("start-survey-in",
        "GPS Time Mode Survey-In (bepaalt de vaste antennepositie, schakelt daarna zelf over naar "
        "Fixed Mode) - alleen zinvol als de antenne al op haar definitieve plek staat. Idempotent: "
        "veilig om standaard aan te laten staan, checkt eerst de huidige status voordat er iets "
        "gestart wordt (zie GpsLink.h).");
    QCommandLineOption surveyInMinDurOpt("survey-in-min-duration-s",
        "Minimale Survey-In-duur in seconden, ongeacht hoe snel de gevraagde nauwkeurigheid al "
        "gehaald wordt (default: 86400 = 24 uur)", "seconds", "86400");
    QCommandLineOption surveyInAccOpt("survey-in-accuracy-m",
        "Vereiste 3D-positienauwkeurigheid in meter om Survey-In te laten stoppen (default: 2.0)",
        "meters", "2.0");
    QCommandLineOption resetSurveyInOpt("reset-survey-in",
        "Forceer een VERSE Survey-In, ook als er al een geldig resultaat/Fixed Mode actief is - "
        "gebruik dit bewust nadat de antenne fysiek verplaatst is. Impliceert --start-survey-in.");
    QCommandLineOption verifyPositionOpt("verify-position",
        "Controleer bij ELKE opstart automatisch of de antenne verplaatst is t.o.v. de eerder "
        "opgeslagen positie (kort terug naar normale 3D-fix, positie middelen, vergelijken) - "
        "schakelt bij een (kleine) afwijking vanzelf een nieuwe Survey-In in, anders wordt de "
        "bekende positie hersteld. Impliceert --start-survey-in-gedrag. Kost een korte Time "
        "Mode-onderbreking bij elke herstart terwijl de module al Fixed staat (zie "
        "TimeModeSupervisor.h) - daarom bewust NIET de --start-survey-in-default.");
    QCommandLineOption verifyThresholdOpt("verify-position-threshold-m",
        "Vanaf hoeveel meter afwijking --verify-position de antenne als verplaatst beschouwt "
        "(default: 5.0)", "meters", "5.0");
    parser.addOption(fllOpt);
    parser.addOption(gpsOpt);
    parser.addOption(surveyInOpt);
    parser.addOption(surveyInMinDurOpt);
    parser.addOption(surveyInAccOpt);
    parser.addOption(resetSurveyInOpt);
    parser.addOption(verifyPositionOpt);
    parser.addOption(verifyThresholdOpt);
    parser.process(app);

    FllLink fllLink;
    GpsLink gpsLink;
    GpsdoModel gpsdoModel;
    gpsdoModel.attachFllLink(&fllLink);
    gpsdoModel.attachGpsLink(&gpsLink);

    // Alleen aangemaakt als --verify-position gevraagd is; &app als parent
    // zodat de levensduur gewoon de hele app.exec() overspant, net als
    // fllLink/gpsLink/gpsdoModel hierboven (geen handmatige delete nodig).
    TimeModeSupervisor *timeModeSupervisor = nullptr;
    if (parser.isSet(verifyPositionOpt))
        timeModeSupervisor = new TimeModeSupervisor(&gpsLink, &app);

    if (parser.isSet(fllOpt)) {
        if (!fllLink.open(parser.value(fllOpt)))
            QTextStream(stderr) << "Waarschuwing: kon FLL-seriele poort niet openen: " << parser.value(fllOpt) << "\n";
    } else {
        QTextStream(stderr) << "Waarschuwing: geen --fll opgegeven, FLL-tegel blijft leeg.\n";
    }

    // Puur diagnostisch: laat in stderr zien welke Time Mode-status de
    // module daadwerkelijk terugmeldt (0=Disabled/1=Survey-In/2=Fixed),
    // ongeacht of dat tot een actie leidt.
    QObject::connect(&gpsLink, &GpsLink::timeModeReported, &app, [](quint8 mode) {
        static const char *names[] = {"Disabled", "Survey-In", "Fixed"};
        const QString name = mode < 3 ? QString::fromLatin1(names[mode]) : QStringLiteral("onbekend (%1)").arg(mode);
        QTextStream(stderr) << "GPS Time Mode (huidige stand): " << name << "\n";
    });

    if (parser.isSet(gpsOpt)) {
        if (gpsLink.open(parser.value(gpsOpt))) {
            const bool wantReset = parser.isSet(resetSurveyInOpt);
            const bool wantVerify = parser.isSet(verifyPositionOpt);
            // --verify-position impliceert hetzelfde "zorg dat er uiteindelijk
            // een Survey-In/Fixed Mode actief is"-gedrag als --start-survey-in
            // (zie de vlag-beschrijving hierboven) — geen aparte
            // --start-survey-in ernaast nodig.
            if (wantReset || wantVerify || parser.isSet(surveyInOpt)) {
                const quint32 minDurationS = parser.value(surveyInMinDurOpt).toUInt();
                const double accuracyM = parser.value(surveyInAccOpt).toDouble();
                // Vereiste 3D-variantie in mm² = (gewenste stddev in mm)^2.
                const double accuracyMm = accuracyM * 1000.0;
                const quint32 varLimitMm2 = static_cast<quint32>(accuracyMm * accuracyMm);

                if (wantReset) {
                    // Bewuste "antenne verplaatst"-actie: eerst terug naar
                    // Disabled, zodat de daaropvolgende requestAutoSurveyIn()
                    // (die immers alleen bij Disabled iets doet) gegarandeerd
                    // een verse meting start i.p.v. het oude resultaat te
                    // laten staan.
                    gpsLink.disableTimeMode();
                    QTextStream(stderr) << "Time Mode teruggezet naar Disabled (--reset-survey-in) — "
                                            "start een verse Survey-In.\n";
                }

                if (wantVerify) {
                    const double thresholdM = parser.value(verifyThresholdOpt).toDouble();
                    timeModeSupervisor->begin(minDurationS, varLimitMm2, thresholdM);
                    QTextStream(stderr) << "Automatische positieverificatie actief (--verify-position): "
                                            "drempel " << thresholdM << "m, min. Survey-In-duur "
                                         << minDurationS << "s, doel-nauwkeurigheid " << accuracyM << "m\n";
                } else if (gpsLink.requestAutoSurveyIn(minDurationS, varLimitMm2)) {
                    QTextStream(stderr) << "Survey-In-status opgevraagd (start automatisch als nog "
                                            "Disabled): min " << minDurationS << "s, doel-nauwkeurigheid "
                                         << accuracyM << "m\n";
                } else {
                    QTextStream(stderr) << "Waarschuwing: kon Survey-In-status niet opvragen.\n";
                }
            }
        } else {
            QTextStream(stderr) << "Waarschuwing: kon GPS-seriele poort niet openen: " << parser.value(gpsOpt) << "\n";
        }
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
