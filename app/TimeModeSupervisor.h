#pragma once
//
// TimeModeSupervisor.h — optionele, opt-in orkestratie boven op GpsLink's
// GPS Time Mode-functies (zie GpsLink.h), voor automatische "is de antenne
// verplaatst?"-verificatie bij het opstarten van de app.
//
// Waarom dit een eigen klasse is en niet in GpsLink/GpsdoModel zit:
//  - GpsLink is bewust een dunne UBX-protocollaag (parseert berichten,
//    stuurt commando's, geen beleid).
//  - GpsdoModel is bewust een pure vertaallaag naar QML, doet zelf geen I/O.
//  - Dit hier IS beleid — een state machine met een timeout, een drempel-
//    waarde-beslissing en persistente opslag — dat hoort in geen van beide
//    thuis.
//
// Wat dit oplost: GpsLink::requestAutoSurveyIn() (zie GpsLink.h) is bewust
// zo ontworpen dat een simpele app-herstart een lopende Survey-In of een
// reeds bereikte Fixed Mode NOOIT verstoort — dat is de veilige default.
// Maar dat betekent ook dat er, eenmaal Fixed, geen enkele controle meer is
// of de antenne ondertussen fysiek verplaatst is: de ontvanger berekent in
// Fixed Mode geen onafhankelijke positie meer om tegen te vergelijken (zie
// GpsLink.h bij requestAutoSurveyIn()). Deze klasse implementeert wat daar
// eerder bewust WEL als "geen automatische heuristiek" werd afgewezen, maar
// nu expliciet door Wilfred gevraagd is: bij het opstarten, als de module al
// Fixed staat, kort teruggaan naar een gewone (Disabled) 3D-fix, een paar
// verse posities middelen, en pas dan beslissen.
//
// State machine, getriggerd door begin():
//  1. We pollen de huidige Time Mode via GpsLink::requestAutoSurveyIn() —
//     dat commando regelt zelf al de Disabled- en Survey-In-lopend-gevallen
//     (start Survey-In resp. laat met rust, ongewijzigd gedrag).
//  2. We luisteren ONAFHANKELIJK daarvan zelf ook naar
//     GpsLink::timeModeReported(). Zien we timeMode==2 (Fixed):
//       a. Geen eerder opgeslagen positie bekend (allereerste run met deze
//          functie, of het state-bestand is weg)? Dan is er niets om tegen
//          te vergelijken — de HUIDIGE (Fixed) ECEF-positie uit de eerst-
//          volgende NAV-SOL wordt gewoon als "laatst bekend goed" opgeslagen
//          en er gebeurt verder niets. Dit voorkomt dat de allereerste keer
//          dat deze feature draait meteen een onnodige heropname triggert.
//       b. Wel een opgeslagen positie? Dan pas echt verifiëren:
//          - disableTimeMode() — module gaat weer een onafhankelijke 3D-fix
//            berekenen (zoals vóór Survey-In).
//          - Verzamel tot kMaxVerifySamples verse fixUpdated()-metingen met
//            fixOk && hasEcef, met een timeout van kVerifyTimeoutMs zodat we
//            nooit voor altijd blijven wachten als de fix om wat voor reden
//            dan ook wegblijft.
//          - Te weinig samples binnen de timeout (antenne/kabel probleem,
//            slecht zicht)? Dan NIET zomaar gokken: gewoon de oude, bekende
//            positie terugzetten via setFixedPosition() zonder verdere
//            actie — veiliger dan een halve/onbetrouwbare meting gebruiken
//            om over "verplaatst" te beslissen.
//          - Genoeg samples? Middel ze, bereken de 3D-afstand (in meters)
//            tot de opgeslagen positie.
//              * Kleiner dan de drempelwaarde: niet verplaatst -> gewoon de
//                oude, nauwkeurige positie terugzetten met setFixedPosition()
//                (geen nieuwe dagenlange Survey-In nodig).
//              * Groter of gelijk: waarschijnlijk verplaatst -> een verse
//                Survey-In starten (startSurveyIn(), dezelfde parameters als
//                de reguliere --start-survey-in-flag).
//  3. Elke keer dat een Survey-In daadwerkelijk voltooit (surveyInUpdated
//     met valid==true && active==false) wordt het resultaat opgeslagen als
//     de nieuwe "laatst bekend goede" positie — dit dekt zowel de eerste-
//     keer-Survey-In (stap 2a) als een hernieuwde Survey-In na een
//     gedetecteerde verplaatsing (stap 2b).
//
// Persistente opslag: een klein JSON-bestand onder
// QStandardPaths::AppDataLocation. Nodig omdat de module zelf zijn Time
// Mode-staat alleen in RAM onthoudt (zie GpsLink.h) — bij een stroomloze
// USB-module (bv. de Pi zelf herstart, of de module losgekoppeld) is er dus
// verder niets meer om tegen te vergelijken zonder deze eigen opslag.
//
// Bewust WEL opt-in via een aparte CLI-vlag (--verify-position, zie
// main.cpp) i.p.v. het standaardgedrag van --start-survey-in stilzwijgend
// te veranderen: dit introduceert een echte trade-off die de zojuist
// gebouwde zero-disruption default (GpsLink::requestAutoSurveyIn()) niet
// heeft — bij elke app-herstart terwijl de module al Fixed staat, verliest
// Time Mode voor de duur van de verificatie (typisch enkele tientallen
// seconden) zijn volle nauwkeurigheid. Voor een module die weken/maanden
// achter elkaar blijft draaien is dat zelden een probleem; voor iemand die
// tijdens ontwikkeling de app vaak herstart, wel.

#include <QObject>
#include <QString>
#include <QTimer>

#include "GpsLink.h"

class TimeModeSupervisor : public QObject {
    Q_OBJECT
public:
    explicit TimeModeSupervisor(GpsLink *gpsLink, QObject *parent = nullptr);

    // Start de state machine hierboven. minDurationSeconds/varLimitMm2:
    // zelfde betekenis als bij GpsLink::startSurveyIn() — gebruikt zowel
    // voor de eerste Survey-In (via requestAutoSurveyIn()) als voor een
    // hernieuwde Survey-In na een gedetecteerde verplaatsing.
    // moveThresholdMeters: 3D-afstand vanaf de opgeslagen positie waarboven
    // de antenne als "verplaatst" wordt beschouwd.
    void begin(quint32 minDurationSeconds, quint32 varLimitMm2, double moveThresholdMeters);

private slots:
    void onTimeModeReported(quint8 timeMode);
    void onFixUpdated(const GpsFix &fix);
    void onSurveyInUpdated(const SurveyInStatus &status);
    void onVerifyTimeout();

private:
    enum class State {
        Idle,
        CollectingVerificationSamples,
    };

    static QString stateFilePath();
    bool loadPersistedPosition(qint32 &xcm, qint32 &ycm, qint32 &zcm, quint32 &varMm2) const;
    void savePersistedPosition(qint32 xcm, qint32 ycm, qint32 zcm, quint32 varMm2);
    void finishVerification();

    GpsLink *m_gpsLink = nullptr;
    State    m_state = State::Idle;

    // Zodra we weten dat er een state-bestand bestaat (via een geslaagde
    // load of save), hoeft onFixUpdated() niet bij elke fix (1Hz) opnieuw
    // stiekem te controleren of het bootstrap-pad nog van toepassing is.
    bool m_persistedFileConfirmed = false;

    quint32 m_minDurationSeconds = 0;
    quint32 m_varLimitMm2 = 0;
    double  m_moveThresholdMeters = 5.0;

    // Opgeslagen referentiepositie waar de lopende verificatie tegen
    // vergelijkt — apart van wat er evt. later opnieuw uit het state-
    // bestand geladen wordt, zodat een verificatie die al bezig is niet
    // halverwege een andere referentie krijgt.
    qint32  m_verifyAgainstXcm = 0;
    qint32  m_verifyAgainstYcm = 0;
    qint32  m_verifyAgainstZcm = 0;
    quint32 m_verifyAgainstVarMm2 = 0;

    // Sommen i.p.v. een lijst met samples — genoeg voor een simpel
    // gemiddelde, geen noodzaak om de losse metingen te bewaren.
    qint64 m_sampleSumXcm = 0;
    qint64 m_sampleSumYcm = 0;
    qint64 m_sampleSumZcm = 0;
    int    m_sampleCount = 0;

    QTimer m_verifyTimeout;

    // Onthoudt wat het laatst NAAR SCHIJF geschreven is, zodat
    // onSurveyInUpdated() (dat na een voltooide Survey-In elke seconde
    // dezelfde valid&&!active-status kan blijven binnenkrijgen zolang de
    // module Fixed staat) niet bij elke identieke update opnieuw naar de
    // SD-kaart schrijft.
    bool    m_lastWrittenKnown = false;
    qint32  m_lastWrittenXcm = 0;
    qint32  m_lastWrittenYcm = 0;
    qint32  m_lastWrittenZcm = 0;
    quint32 m_lastWrittenVarMm2 = 0;

    // Genoeg om ruis in een enkele fix (typisch een paar meter zonder
    // Survey-In-middeling) voldoende uit te middelen, zonder de
    // verificatie nodeloos lang te laten duren.
    static constexpr int kMaxVerifySamples = 20;
    static constexpr int kVerifyTimeoutMs = 90000; // 90s
    // Minimum aantal samples om nog een zinnige "verplaatst?"-beslissing op
    // te baseren — bij minder gewoon veilig terugvallen op de oude positie.
    static constexpr int kMinVerifySamples = 5;
};
