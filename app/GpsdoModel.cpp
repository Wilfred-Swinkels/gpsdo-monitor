// GpsdoModel.cpp
//
// Zie GpsdoModel.h voor het waarom van de aanpak. Deze file bevat alleen
// "vertaal"-logica (ruwe FllStatus/GpsFix/GpsSatellite -> weergave-strings
// en -lijsten), geen I/O.

#include "GpsdoModel.h"

#include <QtMath>
#include <QDateTime>
#include <cmath>

GpsdoModel::GpsdoModel(QObject *parent) : QObject(parent) {
}

void GpsdoModel::attachFllLink(FllLink *link) {
    connect(link, &FllLink::statusUpdated, this, &GpsdoModel::onFllStatus);
    connect(link, &FllLink::lockAcquired, this, &GpsdoModel::lockAcquired);
    connect(link, &FllLink::lockLost, this, &GpsdoModel::lockLost);
    connect(link, &FllLink::rawLineReceived, this, &GpsdoModel::onRawFllLine);
}

void GpsdoModel::attachGpsLink(GpsLink *link) {
    connect(link, &GpsLink::fixUpdated, this, &GpsdoModel::onGpsFix);
    connect(link, &GpsLink::satellitesUpdated, this, &GpsdoModel::onGpsSatellites);
}

void GpsdoModel::onFllStatus(const FllStatus &status) {
    m_fllStatus = status;
    emit fllChanged();

    if (status.valid) {
        // Eerste keer dat we "L" zien sinds het starten van de app: dit is
        // het ankerpunt voor de "sinds lock"-uitlezing op de
        // nauwkeurigheid-trendpagina. Geen persistente opslag, dus na een
        // herstart begint dit gewoon opnieuw — eerlijker dan doen alsof we
        // weten wanneer het écht gelockt is (dat kan al veel eerder zijn
        // geweest, vóór deze app draaide).
        if (status.state == QLatin1Char('L') && m_lockStartEpoch == 0.0) {
            m_lockStartEpoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        }
        pushAccHistoryPoint();

        // Lopend gemiddelde, ALLEEN over samples tijdens een ononderbroken
        // "L"-periode — zie computeAccuracyAvgValue()/accuracyAvgText()
        // hieronder voor het waarom (middelt kwantisatieruis weg, maar is
        // zinloos tijdens Unlocked/Holdover, dus dan resetten i.p.v. door-
        // middelen met acquisitie-ruis).
        if (status.state == QLatin1Char('L')) {
            if (m_avgAccCount == 0)
                m_avgAccWindowStartEpoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;
            m_avgAccSum += computeAccuracyValue();
            ++m_avgAccCount;
        } else {
            m_avgAccSum = 0.0;
            m_avgAccCount = 0;
            m_avgAccWindowStartEpoch = 0.0;
        }
    }
}

void GpsdoModel::onGpsFix(const GpsFix &fix) {
    m_gpsFix = fix;
    emit gpsChanged();
}

void GpsdoModel::onGpsSatellites(const QList<GpsSatellite> &satellites) {
    m_satellites = satellites;
    emit gpsChanged();
}

void GpsdoModel::onRawFllLine(const QByteArray &line) {
    m_rawFllLine = QString::fromLatin1(line).trimmed();
    emit rawFllLineChanged();
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

double GpsdoModel::computeAccuracyValue() const {
    if (!m_fllStatus.valid)
        return 0.0;
    // freqReadout is nominaal 0x6800 (=160.000.000 pulsen/16s bij 10MHz).
    // Verschil t.o.v. dat nulpunt, als signed 16-bit interpreteren i.v.m.
    // wraparound rond de nominale waarde.
    const qint16 raw = static_cast<qint16>(static_cast<quint16>(m_fllStatus.freqReadout - 0x6800));
    const double deltaFHz = raw / 16.0;      // 1 LSB = 1/16 Hz
    return deltaFHz / 1.0e7;                 // f_nominaal = 10 MHz
}

QString GpsdoModel::accuracyText() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    const double deltaFOverF = computeAccuracyValue();
    const double deltaFHz = deltaFOverF * 1.0e7;
    return QStringLiteral("%1%2 (%3 Hz)")
        .arg(deltaFOverF >= 0 ? QStringLiteral("+") : QString())
        .arg(deltaFOverF, 0, 'e', 2)
        .arg(deltaFHz, 0, 'f', 3);
}

double GpsdoModel::computeAccuracyAvgValue() const {
    if (m_avgAccCount == 0)
        return 0.0;
    return m_avgAccSum / static_cast<double>(m_avgAccCount);
}

// Softwarematig lopend gemiddelde van dezelfde Δf/f-waarde als
// accuracyText(), opgebouwd sinds het begin van de huidige ononderbroken
// "L"-periode. Doel: computeAccuracyValue() is gekwantiseerd in stappen van
// 6,25e-9 (1 LSB van de 16-bit freq.-teller over 16s) — een enkele sample
// kan dus nooit fijner tonen dan dat, en pendelt vaak gewoon tussen 0 en
// ±1 LSB. Door veel van die samples te middelen (klassieke oversampling,
// zelfde principe als een gedithered ADC) verdwijnt die kwantisatiestap
// geleidelijk uit het gemiddelde, en kan de uitlezing na lang genoeg
// middelen (orde grootte uren) dieper dan 1e-9/1e-10 gaan — begrensd door
// de werkelijke ruis van de Rb-bron zelf, niet meer door deze rekenkundige
// vloer. Bewust GEEN wijziging aan de FLL-averaging-mode (M01/M02) — dat
// zou de acquisitie/lock-strategie raken (zie FllLink), dit is puur een
// weergave-berekening op de al binnenkomende statuslijnen.
QString GpsdoModel::accuracyAvgText() const {
    if (m_avgAccCount == 0)
        return QStringLiteral("gem: wacht op ononderbroken lock");

    const double avg = computeAccuracyAvgValue();
    const double windowSec = QDateTime::currentMSecsSinceEpoch() / 1000.0 - m_avgAccWindowStartEpoch;

    QString windowText;
    if (windowSec < 90.0) {
        windowText = QStringLiteral("%1s").arg(qRound(windowSec));
    } else if (windowSec < 3600.0) {
        windowText = QStringLiteral("%1m").arg(qRound(windowSec / 60.0));
    } else {
        const int hours = static_cast<int>(windowSec / 3600.0);
        const int mins = qRound(std::fmod(windowSec, 3600.0) / 60.0);
        windowText = QStringLiteral("%1u%2m").arg(hours).arg(mins, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("gem. %1 (%2x): %3%4")
        .arg(windowText)
        .arg(m_avgAccCount)
        .arg(avg >= 0 ? QStringLiteral("+") : QString())
        .arg(avg, 0, 'e', 3);
}

QString GpsdoModel::freqReadoutHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.freqReadout, 4, 16, QLatin1Char('0')).toUpper();
}

QString GpsdoModel::sampleCounterHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.sampleCounter, 4, 16, QLatin1Char('0')).toUpper();
}

QString GpsdoModel::accumDiffHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    const qint16 v = m_fllStatus.accumFreqDiff;
    const quint16 mag = static_cast<quint16>(v < 0 ? -v : v);
    const QString sign = v < 0 ? QStringLiteral("-") : QStringLiteral("+");
    const QString hex = QStringLiteral("%1").arg(mag, 4, 16, QLatin1Char('0')).toUpper();
    return sign + QStringLiteral("0x") + hex;
}

QString GpsdoModel::holdoverHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.holdoverCounter, 4, 16, QLatin1Char('0')).toUpper();
}

QString GpsdoModel::alarmText() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    if (m_fllStatus.alarmLatch == QLatin1Char('.'))
        return QStringLiteral("Geen alarm");
    return QString(m_fllStatus.alarmLatch);
}

void GpsdoModel::pushAccHistoryPoint() {
    const double epoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    QVariantMap point;
    point.insert(QStringLiteral("t"), epoch);
    point.insert(QStringLiteral("v"), computeAccuracyValue());
    m_accHistory.append(point);

    // Begrens op de laatste 24 uur, met een harde backstop op het aantal
    // punten (defensief, mocht de systeemklok bv. door een NTP-sprong raar
    // springen).
    const double cutoff = epoch - 24.0 * 3600.0;
    while (!m_accHistory.isEmpty() &&
           m_accHistory.constFirst().toMap().value(QStringLiteral("t")).toDouble() < cutoff) {
        m_accHistory.removeFirst();
    }
    while (m_accHistory.size() > 8000) {
        m_accHistory.removeFirst();
    }

    emit accHistoryChanged();
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

QVariantList GpsdoModel::satellites() const {
    QVariantList list;
    list.reserve(m_satellites.size());
    for (const auto &s : m_satellites) {
        QVariantMap m;
        m.insert(QStringLiteral("svid"), s.svid);
        m.insert(QStringLiteral("elevationDeg"), s.elevationDeg);
        m.insert(QStringLiteral("azimuthDeg"), s.azimuthDeg);
        m.insert(QStringLiteral("cno"), s.cno);
        m.insert(QStringLiteral("usedInFix"), s.usedInFix);
        m.insert(QStringLiteral("unhealthy"), s.unhealthy);
        list.append(m);
    }
    return list;
}
