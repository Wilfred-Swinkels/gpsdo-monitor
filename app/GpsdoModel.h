#pragma once
//
// GpsdoModel.h — centraal datamodel dat FllLink en GpsLink samenbrengt tot
// QML-vriendelijke properties voor alle 8 pagina's van de UI. Doet zelf geen
// I/O — puur een "vertaallaag" tussen de twee seriële links en de UI.
//
// Uitgebreid t.o.v. de eerste versie (die alleen Overview voedde) met:
//  - de volledige satellietenlijst (skyplot + GPS-fix-balkjes)
//  - gedetailleerde FLL-velden + de ruwe statusregel (FLL-state-pagina)
//  - een groeiende Δf/f-geschiedenis (nauwkeurigheid-trendpagina)
//  - lamp-/xtal-spanning + een 1x/uur-lampspanning-veroudering-geschiedenis
//    (MCP3426, PCB + bekabeling klaar 18-08-2026, zie attachAdc())
//  - een "Base plate"-temperatuur (TSic 506F, zie attachTsic506())
//  - BITE-lock-detect ("Atomic lock"/"Warm-up", zie attachBiteLock())

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantList>
#include <QVariantMap>

#include "FllLink.h"
#include "GpsLink.h"
#include "Tsic506Driver.h"
#include "Mcp3426Adc.h"
#include "BiteLockDriver.h"

class GpsdoModel : public QObject {
    Q_OBJECT

    // --- VE2ZAZ / FllLink ----------------------------------------------
    Q_PROPERTY(bool hasFllData READ hasFllData NOTIFY fllChanged)
    Q_PROPERTY(QString lockState READ lockState NOTIFY fllChanged)     // "L"/"U"/"H"/"D"/"—"
    Q_PROPERTY(QString lockLabel READ lockLabel NOTIFY fllChanged)     // "Locked"/... /"Geen data"
    Q_PROPERTY(QString lockSubText READ lockSubText NOTIFY fllChanged)
    Q_PROPERTY(QString dacHex READ dacHex NOTIFY fllChanged)
    Q_PROPERTY(QString accuracyText READ accuracyText NOTIFY fllChanged)
    // Langzaam verfijnend gemiddelde van accuracyText's onderliggende
    // Δf/f-waarde, opgebouwd sinds het begin van de HUIDIGE ononderbroken
    // L-periode (reset bij elke Unlocked/Holdover) — middelt de 6,25e-9-
    // kwantisatie van een losse 16s-sample geleidelijk weg. Zie de
    // toelichting bij computeAccuracyAvgValue() in de .cpp.
    Q_PROPERTY(QString accuracyAvgText READ accuracyAvgText NOTIFY fllChanged)

    // Gedetailleerde FLL-velden, voor de FLL-state-pagina (osc-grid + status-tiles).
    Q_PROPERTY(QString freqReadoutHex READ freqReadoutHex NOTIFY fllChanged)
    Q_PROPERTY(QString sampleCounterHex READ sampleCounterHex NOTIFY fllChanged)
    Q_PROPERTY(QString accumDiffHex READ accumDiffHex NOTIFY fllChanged)
    Q_PROPERTY(QString holdoverHex READ holdoverHex NOTIFY fllChanged)
    Q_PROPERTY(QString alarmText READ alarmText NOTIFY fllChanged)
    Q_PROPERTY(QString rawFllLine READ rawFllLine NOTIFY rawFllLineChanged)

    // --- LEA-5T / GpsLink -------------------------------------------------
    Q_PROPERTY(bool hasGpsData READ hasGpsData NOTIFY gpsChanged)
    Q_PROPERTY(int satsUsed READ satsUsed NOTIFY gpsChanged)
    Q_PROPERTY(int satsVisible READ satsVisible NOTIFY gpsChanged)
    Q_PROPERTY(QString snrAvgText READ snrAvgText NOTIFY gpsChanged)
    Q_PROPERTY(QString fixTypeText READ fixTypeText NOTIFY gpsChanged)
    Q_PROPERTY(QString hdopText READ hdopText NOTIFY gpsChanged)
    // Volledige satellietenlijst als QVariantList van QVariantMap
    // {svid,elevationDeg,azimuthDeg,cno,usedInFix,unhealthy} — voor de
    // skyplot (pagina 2) en het balkjesdiagram op de GPS-fix-pagina (4).
    Q_PROPERTY(QVariantList satellites READ satellites NOTIFY gpsChanged)
    // Waarnemerspositie (NAV-POSLLH), voor de sterrenbeelden-overlay op de
    // skyplot-pagina — RA/Dec -> Alt/Az vereist de echte breedte-/lengtegraad.
    Q_PROPERTY(bool hasGpsPosition READ hasGpsPosition NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLatitude READ gpsLatitude NOTIFY gpsChanged)
    Q_PROPERTY(double gpsLongitude READ gpsLongitude NOTIFY gpsChanged)

    // Time Mode Survey-In-voortgang (UBX-TIM-SVIN) — start automatisch
    // zodra --gps opgegeven is (zie main.cpp/TimeModeSupervisor.h). Blijft
    // op de "geen data"-staat staan zolang er nog geen Survey-In geweest is.
    Q_PROPERTY(bool surveyInActive READ surveyInActive NOTIFY surveyInChanged)
    Q_PROPERTY(bool surveyInValid READ surveyInValid NOTIFY surveyInChanged)
    Q_PROPERTY(QString surveyInDurationText READ surveyInDurationText NOTIFY surveyInChanged)
    Q_PROPERTY(QString surveyInAccuracyText READ surveyInAccuracyText NOTIFY surveyInChanged)
    Q_PROPERTY(int surveyInObservations READ surveyInObservations NOTIFY surveyInChanged)

    // --- MCP3426 / lamp-/xtal-spanning — bekabeld en gekoppeld 18-08-2026 ---
    // CH1 = LAMP VOLTS (J1-pin 5), CH2 = CRYSTAL VOLTS MONITOR (J1-pin 9),
    // besloten 17-08-2026, zie de "Spanningsdelers"-sectie in de project-
    // brief. lampVoltageText()/xtalVoltageText() geven "—" terug totdat de
    // eerste voltageRead() binnen is (zie onAdcVoltage()).
    Q_PROPERTY(bool hasAdcData READ hasAdcData NOTIFY adcChanged)
    Q_PROPERTY(QString lampVoltageText READ lampVoltageText NOTIFY adcChanged)
    Q_PROPERTY(QString xtalVoltageText READ xtalVoltageText NOTIFY adcChanged)
    // Lange-termijn-veroudering-geschiedenis van de lampspanning, {t, v}
    // net als accHistory/tempHistory — maar bewust MAAR 1 punt/uur (zie
    // pushLampHistoryPoint()), want dit dient de trage lamp-veroudering-
    // trend (LampAgingPage.qml, zoombaar t/m 30 dagen/"Alles"), niet de
    // live spanning. Daarom ook GEEN 24u-tijd-cutoff zoals accHistory/
    // tempHistory (die zou de 30d/Alles-zoom-opties zinloos maken) — alleen
    // een ruime harde punten-cap.
    Q_PROPERTY(QVariantList lampHistory READ lampHistory NOTIFY adcChanged)

    // --- TSic 506F / "Base plate"-temperatuur -------------------------------
    // Empirisch gevalideerd op GPIO17 (18-08-2026, zie Tsic506Driver.h voor
    // de volledige bugfix-geschiedenis). tempText() geeft de kant-en-klare
    // weergavetekst terug (bv. "22.0°C", inclusief gradensymbool+C, zoals
    // afgesproken) i.p.v. een los double-property + QML-formattering, naar
    // analogie van lampVoltageText()/xtalVoltageText() hierboven.
    Q_PROPERTY(bool hasTsicData READ hasTsicData NOTIFY tsicChanged)
    Q_PROPERTY(QString tempText READ tempText NOTIFY tsicChanged)
    // Geschiedenis voor de temperatuur-trendpagina — zelfde {t, v}-vorm als
    // accHistory/lampHistory, maar bewust NIET op elke 10Hz-sample van de
    // sensor gepusht (dat zou de 24u/8000-punten-begrenzing binnen enkele
    // minuten vollopen) — zie pushTempHistoryPoint() in de .cpp voor de
    // throttle.
    Q_PROPERTY(QVariantList tempHistory READ tempHistory NOTIFY tempHistoryChanged)

    // --- BITE-lock-detect (LPRO-101 J1-pin 6) op GPIO27 ---------------------
    // Besloten 17-08-2026, bekabeld en gekoppeld 18-08-2026 (zie
    // BiteLockDriver.h). Vervangt de fix-type-tegel op het Overview-scherm
    // (fix-type blijft gewoon staan op de GPS-fix-pagina). biteLockText()
    // geeft de al-vertaalde UI-tekst terug — "Atomic lock" (LOW/locked) of
    // "Warm-up" (HIGH/unlocked), beide bevestigd door Wilfred 17-08-2026 —
    // zodat QML niet zelf hoeft te vertalen.
    Q_PROPERTY(bool hasBiteLockData READ hasBiteLockData NOTIFY biteLockChanged)
    Q_PROPERTY(QString biteLockText READ biteLockText NOTIFY biteLockChanged)

    // --- Nauwkeurigheid-geschiedenis (trendpagina 7) ------------------------
    // Lijst van {t: seconden-sinds-epoch (double), v: instantane Δf/f
    // (double), avg: lopend gemiddelde Δf/f sinds het begin van de huidige
    // ononderbroken "L"-periode (double, alleen aanwezig als er op dat
    // moment al een gemiddelde is — zie computeAccuracyAvgValue())}, gevuld
    // zodra er FLL-statusregels binnenkomen. Groeit vanaf het moment dat de
    // app start (geen persistente opslag — dus bij herstart begint de
    // geschiedenis opnieuw), begrensd op de laatste 24 uur. AccuracyTrend-
    // Page.qml plot de "avg"-reeks (niet "v") — die is niet gekwantiseerd
    // in stappen van 6,25e-9 zoals de rauwe instant-waarde, zie
    // computeAccuracyAvgValue() hieronder voor het waarom.
    Q_PROPERTY(QVariantList accHistory READ accHistory NOTIFY accHistoryChanged)
    Q_PROPERTY(double lockStartEpoch READ lockStartEpoch NOTIFY accHistoryChanged)

public:
    explicit GpsdoModel(QObject *parent = nullptr);

    void attachFllLink(FllLink *link);
    void attachGpsLink(GpsLink *link);
    void attachTsic506(Tsic506Driver *driver);
    void attachAdc(Mcp3426Adc *adc);
    void attachBiteLock(BiteLockDriver *driver);

    // Persistentie voor lampHistory (18-08-2026, n.a.v. Wilfreds terechte
    // opmerking: 1 sample/uur heeft alleen zin als de geschiedenis een
    // herstart overleeft — anders fragmenteert de veroudering-trend bij
    // elke reboot/service-restart). Laadt bestaande punten uit een simpel
    // "epoch,volt"-CSV-bestand (indien aanwezig) in m_lampHistory, en zet
    // m_lastLampHistoryPushEpoch op het laatste geladen punt zodat de
    // 1x/uur-throttle gewoon doorloopt vanaf waar hij gebleven was i.p.v.
    // meteen een nieuw punt te pushen na het laden. Aanroepen VOOR
    // attachAdc()/adc.start(), zie main.cpp. Ontbrekend bestand is geen
    // fout — gewoon leeg beginnen (eerste run).
    void loadLampHistory(const QString &path);

    bool hasFllData() const { return m_fllStatus.valid; }
    QString lockState() const;
    QString lockLabel() const;
    QString lockSubText() const;
    QString dacHex() const;
    QString accuracyText() const;
    QString accuracyAvgText() const;
    QString freqReadoutHex() const;
    QString sampleCounterHex() const;
    QString accumDiffHex() const;
    QString holdoverHex() const;
    QString alarmText() const;
    QString rawFllLine() const { return m_rawFllLine; }

    bool hasGpsData() const { return m_gpsFix.valid; }
    int satsUsed() const { return m_gpsFix.numSatellites; }
    int satsVisible() const { return m_satellites.size(); }
    QString snrAvgText() const;
    QString fixTypeText() const;
    QString hdopText() const;
    QVariantList satellites() const;
    bool hasGpsPosition() const { return m_gpsFix.hasPosition; }
    double gpsLatitude() const { return m_gpsFix.latitudeDeg; }
    double gpsLongitude() const { return m_gpsFix.longitudeDeg; }

    bool surveyInActive() const { return m_surveyIn.active; }
    bool surveyInValid() const { return m_surveyIn.valid; }
    QString surveyInDurationText() const;
    QString surveyInAccuracyText() const;
    int surveyInObservations() const { return static_cast<int>(m_surveyIn.observations); }

    bool hasAdcData() const { return m_hasLampData || m_hasXtalData; }
    QString lampVoltageText() const;
    QString xtalVoltageText() const;
    QVariantList lampHistory() const { return m_lampHistory; }

    QVariantList accHistory() const { return m_accHistory; }
    double lockStartEpoch() const { return m_lockStartEpoch; }

    bool hasTsicData() const { return m_hasTsicData; }
    QString tempText() const;
    QVariantList tempHistory() const { return m_tempHistory; }

    bool hasBiteLockData() const { return m_hasBiteLockData; }
    QString biteLockText() const;

signals:
    void fllChanged();
    void gpsChanged();
    void surveyInChanged();
    void adcChanged();
    void lockAcquired();
    void lockLost();
    void rawFllLineChanged();
    void accHistoryChanged();
    void tsicChanged();
    void tempHistoryChanged();
    void biteLockChanged();

private slots:
    void onFllStatus(const FllStatus &status);
    void onGpsFix(const GpsFix &fix);
    void onGpsSatellites(const QList<GpsSatellite> &satellites);
    void onSurveyIn(const SurveyInStatus &status);
    void onRawFllLine(const QByteArray &line);
    void onTsicTemperature(double celsius, quint16 rawValue);
    void onAdcVoltage(const QString &channelName, double voltage, qint16 rawCode);
    void onBiteLockChanged(bool locked);

private:
    double computeAccuracyValue() const; // Δf/f als kommagetal, uit m_fllStatus
    double computeAccuracyAvgValue() const; // lopend gemiddelde sinds huidige L-periode
    void pushAccHistoryPoint();
    void pushTempHistoryPoint(double celsius);
    void pushLampHistoryPoint(double voltage);

    FllStatus m_fllStatus;
    GpsFix m_gpsFix;
    QList<GpsSatellite> m_satellites;
    SurveyInStatus m_surveyIn;
    QString m_rawFllLine;

    // Lopend gemiddelde van computeAccuracyValue(), alleen opgebouwd tijdens
    // een ononderbroken "L"-periode (reset bij elke Unlocked/Holdover — zie
    // GpsdoModel.cpp voor het waarom). Puur software-matig, raakt de
    // FLL-averaging-mode (M01/M02) niet aan.
    double m_avgAccSum = 0.0;
    qint64 m_avgAccCount = 0;
    double m_avgAccWindowStartEpoch = 0.0;

    QVariantList m_accHistory;
    double m_lockStartEpoch = 0.0; // 0 = nog nooit gelockt sinds app-start

    bool m_hasLampData = false;
    double m_lampVoltage = 0.0;
    bool m_hasXtalData = false;
    double m_xtalVoltage = 0.0;
    QVariantList m_lampHistory;
    // Epoch (seconden) van het laatst GEPUSHTE lampHistory-punt — throttle
    // voor pushLampHistoryPoint() (1x/uur, zie .cpp). 0 = nog nooit gepusht.
    double m_lastLampHistoryPushEpoch = 0.0;
    // Bestandspad voor lampHistory-persistentie, gezet door loadLampHistory().
    // Leeg = geen persistentie (bv. als loadLampHistory() nooit aangeroepen
    // is) — pushLampHistoryPoint() slaat het wegschrijven dan gewoon over.
    QString m_lampHistoryFilePath;

    bool m_hasTsicData = false;
    double m_lastTempC = 0.0;
    QVariantList m_tempHistory;
    // Epoch (seconden) van de laatst GEPUSHTE tempHistory-punt — throttle
    // voor pushTempHistoryPoint(), zie .cpp. 0 = nog nooit gepusht.
    double m_lastTempHistoryPushEpoch = 0.0;

    bool m_hasBiteLockData = false;
    bool m_biteLocked = false; // true = LOW = locked ("Atomic lock"), false = HIGH = "Warm-up"
};
