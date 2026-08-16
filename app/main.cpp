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
#include <QTimer>

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
    // ongeacht of dat tot een actie leidt. gotTimeModeResponse hieronder
    // wordt gebruikt door de timeout-check verderop: als deze regel NOOIT
    // verschijnt, betekent dat dat de module niet reageert op onze
    // CFG-TMODE-statusvraag (zie die check voor de mogelijke oorzaken).
    bool gotTimeModeResponse = false;
    QObject::connect(&gpsLink, &GpsLink::timeModeReported, &app, [&gotTimeModeResponse](quint8 mode) {
        gotTimeModeResponse = true;
        static const char *names[] = {"Disabled", "Survey-In", "Fixed"};
        const QString name = mode < 3 ? QString::fromLatin1(names[mode]) : QStringLiteral("onbekend (%1)").arg(mode);
        QTextStream(stderr) << "GPS Time Mode (huidige stand): " << name << "\n";
    });

    // Ook puur diagnostisch: de module bevestigt (ACK) of wijst (NAK) elk
    // CFG-bericht dat WIJ sturen af — dus ook onze CFG-TMODE-commando's.
    // Toegevoegd nadat bleek dat de "huidige stand"-regel hierboven soms
    // helemaal niet verscheen: dit laat zien of de module actief NEE zegt
    // tegen Time Mode, in plaats van dat het stil blijft.
    QObject::connect(&gpsLink, &GpsLink::ackReceived, &app, [](quint8 msgClass, quint8 msgId, bool acked) {
        if (msgClass == 0x06 && msgId == 0x1D) { // CFG-TMODE (legacy, u-blox 5)
            QTextStream(stderr) << "GPS CFG-TMODE " << (acked ? "geaccepteerd (ACK)" : "AFGEWEZEN door module (NAK)") << "\n";
        } else if (msgClass == 0x06 && msgId == 0x3D) { // CFG-TMODE2 (diagnostische testpoll, zie GpsLink.h)
            QTextStream(stderr) << "GPS CFG-TMODE2 (diagnostische testpoll) "
                                 << (acked ? "geaccepteerd (ACK) -- deze 5T ondersteunt blijkbaar TOCH TMODE2!"
                                           : "AFGEWEZEN door module (NAK)")
                                 << "\n";
        }
    });

    // Antwoord op dezelfde diagnostische CFG-TMODE2-poll, maar dan het geval
    // dat de module ECHT met een inhoudelijk CFG-TMODE2-bericht antwoordt
    // i.p.v. een ACK/NAK — nog sterker bewijs dat TMODE2 ondersteund wordt.
    QObject::connect(&gpsLink, &GpsLink::cfgTmode2RawResponseReceived, &app, [](const QByteArray &payload) {
        QTextStream(stderr) << "GPS CFG-TMODE2 (diagnostische testpoll) -- module antwoordde inhoudelijk met "
                             << payload.size() << " databytes: " << payload.toHex(' ')
                             << " -- dit bewijst dat deze module TMODE2 ondersteunt.\n";
    });

    // Puur diagnostisch: laat zien wat de module zelf claimt te zijn/kunnen
    // (MON-VER, automatisch gepolld door GpsLink::open()) — toegevoegd om
    // te kunnen verifiëren of deze fysieke module zichzelf als "timing"-
    // capable identificeert, nadat CFG-TMODE in de praktijk een NAK bleek
    // op te leveren.
    QObject::connect(&gpsLink, &GpsLink::versionInfoReceived, &app,
                      [](const QString &sw, const QString &hw, const QStringList &extensions) {
        QTextStream(stderr) << "GPS MON-VER — software: " << sw << ", hardware: " << hw << "\n";
        for (const QString &ext : extensions)
            QTextStream(stderr) << "GPS MON-VER — extensie: " << ext << "\n";
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
            QTextStream(stderr) << "GPS Time Mode: automatische Survey-In/Fixed-verificatie AANGEVRAAGD "
                                    "(min " << minDurationS << "s, doel-nauwkeurigheid " << accuracyM
                                 << "m, verplaatsingsdrempel " << thresholdM << "m) — wacht op reactie "
                                    "van de module...\n";

            // Diagnostische timeout: als er na 5s nog steeds geen enkele
            // CFG-TMODE-statusregel verschenen is (zie gotTimeModeResponse
            // hierboven), reageert de module helemaal niet op onze
            // statusvraag — dat is dus geen "niet actief"-resultaat maar
            // een falende aanvraag, en dat verschil was zonder deze regel
            // niet te zien in de logs.
            QTimer::singleShot(5000, &app, [&gotTimeModeResponse]() {
                if (!gotTimeModeResponse) {
                    QTextStream(stderr) << "Waarschuwing: geen CFG-TMODE-antwoord ontvangen binnen 5s -- "
                                            "de module reageert niet op de Time Mode-statusvraag. Mogelijke "
                                            "oorzaken: deze module ondersteunt Time Mode toch niet, of er is "
                                            "een protocolprobleem. Zie ook of er een ACK/NAK-regel verschenen "
                                            "is.\n";
                }
            });
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
