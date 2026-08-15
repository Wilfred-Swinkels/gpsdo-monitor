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
// GPS Time Mode (Survey-In / Fixed Mode, zie GpsLink.h/TimeModeSupervisor.h):
// volledig AUTOMATISCH, geen CLI-vlag nodig — op Wilfreds expliciete
// verzoek ("ik wil geen extra cli handling doen, dit moet automatisch gaan
// bij het starten van het programma"). Zodra --gps opgegeven is en de poort
// opent, regelt TimeModeSupervisor bij ELKE app-start zelf:
//   - Module nog Disabled?            -> start een Survey-In.
//   - Survey-In loopt al?             -> laat met rust.
//   - Module staat al Fixed?          -> kort terug naar een gewone 3D-fix,
//     positie verifiëren t.o.v. de eerder opgeslagen referentie; geen
//     (relevante) afwijking -> Fixed Mode direct hersteld; wel een
//     afwijking -> automatisch een verse Survey-In.
// Dit kost dus bij ELKE herstart terwijl de module al Fixed staat een korte
// Time Mode-onderbreking (typisch enkele tientallen seconden) — een
// bewuste, door Wilfred geaccepteerde trade-off t.o.v. compleet stilzitten.
// --survey-in-min-duration-s/--survey-in-accuracy-m/--verify-position-
// threshold-m blijven optionele tuning-vlaggen met zinnige defaults (24u/
// 2.0m/5.0m) — nergens verplicht. --reset-survey-in blijft beschikbaar als
// bewuste, handmatige "ik weet zeker dat de antenne verplaatst is"-actie
// die de automatische drempel-check overslaat.

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
    QCommandLineOption surveyInMinDurOpt("survey-in-min-duration-s",
        "Minimale Survey-In-duur in seconden, ongeacht hoe snel de gevraagde nauwkeurigheid al "
        "gehaald wordt (default: 86400 = 24 uur)", "seconds", "86400");
    QCommandLineOption surveyInAccOpt("survey-in-accuracy-m",
        "Vereiste 3D-positienauwkeurigheid in meter om Survey-In te laten stoppen (default: 2.0)",
        "meters", "2.0");
    QCommandLineOption resetSurveyInOpt("reset-survey-in",
        "Forceer een VERSE Survey-In, ook als er al een geldig resultaat/Fixed Mode actief is - "
        "gebruik dit bewust nadat de antenne fysiek verplaatst is (slaat de automatische "
        "afstandsdrempel-check over).");
    QCommandLineOption verifyThresholdOpt("verify-position-threshold-m",
        "Vanaf hoeveel meter afwijking de automatische opstart-verificatie de antenne als "
        "verplaatst beschouwt (default: 5.0)", "meters", "5.0");
    parser.addOption(fllOpt);
    parser.addOption(gpsOpt);
    parser.addOption(surveyInMinDurOpt);
    parser.addOption(surveyInAccOpt);
    parser.addOption(resetSurveyInOpt);
    parser.addOption(verifyThresholdOpt);
    parser.process(app);

    FllLink fllLink;
    GpsLink gpsLink;
    GpsdoModel gpsdoModel;
    gpsdoModel.attachFllLink(&fllLink);
    gpsdoModel.attachGpsLink(&gpsLink);

    // Onvoorwaardelijk aangemaakt (geen CLI-vlag meer nodig, zie toelichting
    // bovenaan dit bestand) — gewoon een stack-object zoals fllLink/gpsLink/
    // gpsdoModel hierboven, geen QObject-parent nodig (levensduur volgt
    // gewoon de scope van main(), net als de rest). Onschuldig als --gps
    // niet gebruikt wordt: begin() wordt dan hieronder simpelweg nooit
    // aangeroepen, dus er gebeurt niets.
    TimeModeSupervisor timeModeSupervisor(&gpsLink);

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
            const quint32 minDurationS = parser.value(surveyInMinDurOpt).toUInt();
            const double accuracyM = parser.value(surveyInAccOpt).toDouble();
            // Vereiste 3D-variantie in mm² = (gewenste stddev in mm)^2.
            const double accuracyMm = accuracyM * 1000.0;
            const quint32 varLimitMm2 = static_cast<quint32>(accuracyMm * accuracyMm);
            const double thresholdM = parser.value(verifyThresholdOpt).toDouble();

            if (parser.isSet(resetSurveyInOpt)) {
                // Bewuste "antenne verplaatst"-actie: eerst terug naar
                // Disabled, zodat TimeModeSupervisor::begin() hieronder
                // (die bij Disabled altijd een Survey-In start) gegarandeerd
                // een verse meting start i.p.v. het oude resultaat te
                // laten staan of eerst de automatische drempel-check te doen.
                gpsLink.disableTimeMode();
                QTextStream(stderr) << "Time Mode teruggezet naar Disabled (--reset-survey-in) — "
                                        "start een verse Survey-In.\n";
            }

            // Altijd actief, geen CLI-vlag nodig — zie toelichting bovenaan
            // dit bestand.
            timeModeSupervisor.begin(minDurationS, varLimitMm2, thresholdM);
            QTextStream(stderr) << "GPS Time Mode: automatische Survey-In/Fixed-verificatie actief "
                                    "(min " << minDurationS << "s, doel-nauwkeurigheid " << accuracyM
                                 << "m, verplaatsingsdrempel " << thresholdM << "m)\n";
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
