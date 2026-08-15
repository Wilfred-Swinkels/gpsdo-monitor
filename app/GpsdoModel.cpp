// GpsdoModel.cpp
//
// Zie GpsdoModel.h voor het waarom van de aanpak. Deze file bevat alleen
// "vertaal"-logica (ruwe FllStatus/GpsFix/GpsSatellite -> weergave-strings),
// geen I/O.

#include "GpsdoModel.h"

#include <QtMath>

GpsdoModel::GpsdoModel(QObject *parent) : QObject(parent) {
}

void GpsdoModel::attachFllLink(FllLink *link) {
    connect(link, &FllLink::statusUpdated, this, &GpsdoModel::onFllStatus);
    connect(link, &FllLink::lockAcquired, this, &GpsdoModel::lockAcquired);
    connect(link, &FllLink::lockLost, this, &GpsdoModel::lockLost);
}

void GpsdoModel::attachGpsLink(GpsLink *link) {
    connect(link, &GpsLink::fixUpdated, this, &GpsdoModel::onGpsFix);
    connect(link, &GpsLink::satellitesUpdated, this, &GpsdoModel::onGpsSatellites);
}

void GpsdoModel::onFllStatus(const FllStatus &status) {
    m_fllStatus = status;
    emit fllChanged();
}

void GpsdoModel::onGpsFix(const GpsFix &fix) {
    m_gpsFix = fix;
    emit gpsChanged();
}

void GpsdoModel::onGpsSatellites(const QList<GpsSatellite> &satellites) {
    m_satellites = satellites;
    emit gpsChanged();
}

// --- VE2ZAZ / FllLink -------------------------------------------------

QString GpsdoModel::lockState() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QString(m_fllStatus.state);
}

QString GpsdoModel::lockLabel() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("GEEN DATA");
    switch (m_fllStatus.state.toLatin1()) {
    case 'L': return QStringLiteral("LOCKED");
    case 'U': return QStringLiteral("UNLOCKED");
    case 'H': return QStringLiteral("HOLDOVER");
    case 'D': return QStringLiteral("DISABLED");
    default:  return QStringLiteral("ONBEKEND");
    }
}

QString GpsdoModel::lockSubText() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("Wachten op verbinding met VE2ZAZ...");
    switch (m_fllStatus.state.toLatin1()) {
    case 'L': return QStringLiteral("Frequentie gestabiliseerd op GPS-referentie");
    case 'U': return QStringLiteral("Acquisitie bezig — nog geen lock");
    case 'H': return QStringLiteral("GPS-signaal verloren — houdt laatste DAC-waarde vast");
    case 'D': return QStringLiteral("FLL-regeling uitgeschakeld");
    default:  return QStringLiteral("Onbekende status");
    }
}

QString GpsdoModel::dacHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.dacValue, 4, 16, QLatin1Char('0')).toUpper();
}

QString GpsdoModel::accuracyText() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");

    // freqReadout is nominaal 0x6800 (=160.000.000 pulsen/16s bij 10MHz).
    // Verschil t.o.v. dat nulpunt, als signed 16-bit interpreteren i.v.m.
    // wraparound rond de nominale waarde.
    const qint32 raw = static_cast<qint16>(static_cast<quint16>(m_fllStatus.freqReadout - 0x6800));
    const double deltaFHz = raw / 16.0;      // 1 LSB = 1/16 Hz
    const double deltaFOverF = deltaFHz / 1.0e7; // f_nominaal = 10 MHz

    return QStringLiteral("%1%2 (%3 Hz)")
        .arg(deltaFOverF >= 0 ? QStringLiteral("+") : QString())
        .arg(deltaFOverF, 0, 'e', 2)
        .arg(deltaFHz, 0, 'f', 3);
}

// --- LEA-5T / GpsLink ------------------------------------------------

QString GpsdoModel::snrAvgText() const {
    if (m_satellites.isEmpty())
        return QStringLiteral("—");

    int sum = 0;
    int count = 0;
    for (const auto &sat : m_satellites) {
        if (sat.usedInFix) {
            sum += sat.cno;
            ++count;
        }
    }
    if (count == 0)
        return QStringLiteral("—");

    const double avg = static_cast<double>(sum) / count;
    return QStringLiteral("%1 dBHz").arg(avg, 0, 'f', 0);
}

QString GpsdoModel::fixTypeText() const {
    if (!m_gpsFix.valid)
        return QStringLiteral("Geen data");
    if (!m_gpsFix.fixOk)
        return QStringLiteral("Geen fix");
    switch (m_gpsFix.fixType) {
    case 0: return QStringLiteral("Geen fix");
    case 1: return QStringLiteral("Dead-reckoning");
    case 2: return QStringLiteral("2D fix");
    case 3: return QStringLiteral("3D fix");
    case 4: return QStringLiteral("GPS + DR");
    case 5: return QStringLiteral("Alleen tijd");
    default: return QStringLiteral("Onbekend");
    }
}

QString GpsdoModel::hdopText() const {
    if (!m_gpsFix.valid)
        return QStringLiteral("—");
    return QString::number(m_gpsFix.hdop, 'f', 2);
}
