#pragma once
//
// GpsdoModel.h — centraal datamodel dat FllLink en GpsLink samenbrengt tot
// QML-vriendelijke properties voor de Overview-pagina (en later de overige
// pagina's). Doet zelf geen I/O — puur een "vertaallaag" tussen de twee
// seriële links en de UI.
//
// Bewust NIET hier: lamp-/xtal-spanning — de MCP3426 is nog niet bekabeld
// (Wilfred is de PCB nog aan het maken), dus lampVoltageText()/
// xtalVoltageText() geven voorlopig een vaste placeholder terug. Zodra
// Mcp3426Adc aangesloten wordt: attachAdc(Mcp3426Adc*) toevoegen naar
// analogie van attachFllLink()/attachGpsLink(), en deze twee properties
// vullen vanuit de voltageRead()-signalen.

#include <QObject>
#include <QString>
#include <QList>

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

    // --- LEA-5T / GpsLink -------------------------------------------------
    Q_PROPERTY(bool hasGpsData READ hasGpsData NOTIFY gpsChanged)
    Q_PROPERTY(int satsUsed READ satsUsed NOTIFY gpsChanged)
    Q_PROPERTY(int satsVisible READ satsVisible NOTIFY gpsChanged)
    Q_PROPERTY(QString snrAvgText READ snrAvgText NOTIFY gpsChanged)
    Q_PROPERTY(QString fixTypeText READ fixTypeText NOTIFY gpsChanged)
    Q_PROPERTY(QString hdopText READ hdopText NOTIFY gpsChanged)

    // --- MCP3426 / lamp-/xtal-spanning — NOG NIET BEKABELD -----------------
    // Placeholders totdat de PCB klaar is en Mcp3426Adc hierop aangesloten wordt.
    Q_PROPERTY(QString lampVoltageText READ lampVoltageText NOTIFY adcChanged)
    Q_PROPERTY(QString xtalVoltageText READ xtalVoltageText NOTIFY adcChanged)

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

    bool hasGpsData() const { return m_gpsFix.valid; }
    int satsUsed() const { return m_gpsFix.numSatellites; }
    int satsVisible() const { return m_satellites.size(); }
    QString snrAvgText() const;
    QString fixTypeText() const;
    QString hdopText() const;

    QString lampVoltageText() const { return QStringLiteral("—"); }
    QString xtalVoltageText() const { return QStringLiteral("—"); }

signals:
    void fllChanged();
    void gpsChanged();
    void adcChanged();
    void lockAcquired();
    void lockLost();

private slots:
    void onFllStatus(const FllStatus &status);
    void onGpsFix(const GpsFix &fix);
    void onGpsSatellites(const QList<GpsSatellite> &satellites);

private:
    FllStatus m_fllStatus;
    GpsFix m_gpsFix;
    QList<GpsSatellite> m_satellites;
};
