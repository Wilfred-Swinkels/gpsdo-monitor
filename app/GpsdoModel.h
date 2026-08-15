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
//  - een (nog lege) lampspanning-geschiedenis-haak, voor zodra de MCP3426
//    bekabeld is (lampspanning-veroudering-pagina)
//
// Bewust NIET hier: lamp-/xtal-spanning zelf — de MCP3426 is nog niet
// bekabeld (Wilfred is de PCB nog aan het maken), dus lampVoltageText()/
// xtalVoltageText() geven voorlopig een vaste placeholder terug. Zodra
// Mcp3426Adc aangesloten wordt: attachAdc(Mcp3426Adc*) toevoegen naar
// analogie van attachFllLink()/attachGpsLink(), en deze twee properties
// vullen vanuit de voltageRead()-signalen (en lampHistory ook echt vullen).

#include <QObject>
#include <QString>
#include <QList>
#include <QVariantList>
#include <QVariantMap>

#include "FllLink.h"
#include "GpsLink.h"

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

    // Time Mode Survey-In-voortgang (UBX-TIM-SVIN) — alleen zinvol nadat de
    // app met --start-survey-in gestart is (zie main.cpp/GpsLink.h). Blijft
    // op de "geen data"-staat staan als Survey-In niet aan staat.
    Q_PROPERTY(bool surveyInActive READ surveyInActive NOTIFY surveyInChanged)
    Q_PROPERTY(bool surveyInValid READ surveyInValid NOTIFY surveyInChanged)
    Q_PROPERTY(QString surveyInDurationText READ surveyInDurationText NOTIFY surveyInChanged)
    Q_PROPERTY(QString surveyInAccuracyText READ surveyInAccuracyText NOTIFY surveyInChanged)
    Q_PROPERTY(int surveyInObservations READ surveyInObservations NOTIFY surveyInChanged)

    // --- MCP3426 / lamp-/xtal-spanning — NOG NIET BEKABELD -----------------
    Q_PROPERTY(QString lampVoltageText READ lampVoltageText NOTIFY adcChanged)
    Q_PROPERTY(QString xtalVoltageText READ xtalVoltageText NOTIFY adcChanged)
    Q_PROPERTY(QVariantList lampHistory READ lampHistory NOTIFY adcChanged)

    // --- Nauwkeurigheid-geschiedenis (trendpagina 7) ------------------------
    // Lijst van {t: seconden-sinds-epoch (double), v: Δf/f (double)}, gevuld
    // zodra er FLL-statusregels binnenkomen. Groeit vanaf het moment dat de
    // app start (geen persistente opslag — dus bij herstart begint de
    // geschiedenis opnieuw), begrensd op de laatste 24 uur.
    Q_PROPERTY(QVariantList accHistory READ accHistory NOTIFY accHistoryChanged)
    Q_PROPERTY(double lockStartEpoch READ lockStartEpoch NOTIFY accHistoryChanged)

public:
    explicit GpsdoModel(QObject *parent = nullptr);

    void attachFllLink(FllLink *link);
    void attachGpsLink(GpsLink *link);
    // void attachAdc(Mcp3426Adc *adc); // TODO zodra de MCP3426 bekabeld is

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

    QString lampVoltageText() const { return QStringLiteral("—"); }
    QString xtalVoltageText() const { return QStringLiteral("—"); }
    QVariantList lampHistory() const { return m_lampHistory; }

    QVariantList accHistory() const { return m_accHistory; }
    double lockStartEpoch() const { return m_lockStartEpoch; }

signals:
    void fllChanged();
    void gpsChanged();
    void surveyInChanged();
    void adcChanged();
    void lockAcquired();
    void lockLost();
    void rawFllLineChanged();
    void accHistoryChanged();

private slots:
    void onFllStatus(const FllStatus &status);
    void onGpsFix(const GpsFix &fix);
    void onGpsSatellites(const QList<GpsSatellite> &satellites);
    void onSurveyIn(const SurveyInStatus &status);
    void onRawFllLine(const QByteArray &line);

private:
    double computeAccuracyValue() const; // Δf/f als kommagetal, uit m_fllStatus
    double computeAccuracyAvgValue() const; // lopend gemiddelde sinds huidige L-periode
    void pushAccHistoryPoint();

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

    QVariantList m_lampHistory; // blijft leeg totdat de MCP3426 bekabeld is
};
