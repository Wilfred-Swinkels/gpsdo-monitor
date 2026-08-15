#include "TimeModeSupervisor.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTextStream>

#include <cmath>

TimeModeSupervisor::TimeModeSupervisor(GpsLink *gpsLink, QObject *parent)
    : QObject(parent), m_gpsLink(gpsLink) {
    m_verifyTimeout.setSingleShot(true);
    m_verifyTimeout.setInterval(kVerifyTimeoutMs);
    connect(&m_verifyTimeout, &QTimer::timeout, this, &TimeModeSupervisor::onVerifyTimeout);

    connect(m_gpsLink, &GpsLink::timeModeReported, this, &TimeModeSupervisor::onTimeModeReported);
    connect(m_gpsLink, &GpsLink::fixUpdated, this, &TimeModeSupervisor::onFixUpdated);
    connect(m_gpsLink, &GpsLink::surveyInUpdated, this, &TimeModeSupervisor::onSurveyInUpdated);
}

void TimeModeSupervisor::begin(quint32 minDurationSeconds, quint32 varLimitMm2, double moveThresholdMeters) {
    m_minDurationSeconds = minDurationSeconds;
    m_varLimitMm2 = varLimitMm2;
    m_moveThresholdMeters = moveThresholdMeters;

    // Regelt zelf al het Disabled- ("start Survey-In") en Survey-In-lopend-
    // geval ("laat met rust") — ongewijzigd t.o.v. het bestaande, veilige
    // --start-survey-in-gedrag. Wij haken via timeModeReported() hierboven
    // apart in op het Fixed-geval om te verifiëren.
    if (!m_gpsLink->requestAutoSurveyIn(minDurationSeconds, varLimitMm2)) {
        QTextStream(stderr) << "TimeModeSupervisor: kon Time Mode-status niet opvragen "
                                "(--verify-position doet niets deze run).\n";
    }
}

void TimeModeSupervisor::onTimeModeReported(quint8 timeMode) {
    if (timeMode != 2) // alleen Fixed Mode is interessant hier
        return;
    if (m_state != State::Idle)
        return; // verificatie loopt al (of net klaar) -- niet dubbel starten

    qint32 xcm = 0, ycm = 0, zcm = 0;
    quint32 varMm2 = 0;
    if (!loadPersistedPosition(xcm, ycm, zcm, varMm2)) {
        // Allereerste keer dat deze feature draait terwijl de module al
        // Fixed staat (bv. handmatig via --start-survey-in in een eerdere
        // sessie, ver voor TimeModeSupervisor bestond) -- niets om tegen te
        // vergelijken. Wacht op de eerstvolgende NAV-SOL met een geldige
        // ECEF-positie (dat IS de huidige, al gefixeerde referentie) en
        // sla die simpelweg op als startpunt voor toekomstige runs.
        QTextStream(stderr) << "TimeModeSupervisor: module staat al Fixed maar er is nog geen "
                                "opgeslagen referentiepositie -- huidige positie wordt als "
                                "uitgangspunt vastgelegd (geen verificatie deze run).\n";
        return;
    }

    QTextStream(stderr) << "TimeModeSupervisor: module staat Fixed -- verifieer positie t.o.v. "
                            "opgeslagen referentie (kort terug naar normale 3D-fix)...\n";
    m_verifyAgainstXcm = xcm;
    m_verifyAgainstYcm = ycm;
    m_verifyAgainstZcm = zcm;
    m_verifyAgainstVarMm2 = varMm2;
    m_sampleSumXcm = m_sampleSumYcm = m_sampleSumZcm = 0;
    m_sampleCount = 0;
    m_state = State::CollectingVerificationSamples;

    m_gpsLink->disableTimeMode();
    m_verifyTimeout.start();
}

void TimeModeSupervisor::onFixUpdated(const GpsFix &fix) {
    if (!fix.hasEcef)
        return;

    if (m_state == State::Idle) {
        // Bootstrap-pad (zie onTimeModeReported): geen opgeslagen positie
        // bekend. Zolang er nog geen state-bestand bevestigd is, leggen we
        // de eerstvolgende binnenkomende Fixed-positie vast als referentie.
        // m_persistedFileConfirmed voorkomt dat dit elke seconde (1Hz fix-
        // rate) opnieuw een bestandslezing triggert zodra dat eenmaal is
        // vastgesteld.
        if (m_persistedFileConfirmed)
            return;
        qint32 existingX = 0, existingY = 0, existingZ = 0;
        quint32 existingVar = 0;
        if (loadPersistedPosition(existingX, existingY, existingZ, existingVar)) {
            m_persistedFileConfirmed = true;
            return;
        }
        savePersistedPosition(fix.ecefXcm, fix.ecefYcm, fix.ecefZcm, m_varLimitMm2);
        m_persistedFileConfirmed = true;
        return;
    }

    if (m_state != State::CollectingVerificationSamples)
        return;
    if (!fix.fixOk)
        return; // nog geen bruikbare fix na disableTimeMode(), overslaan

    m_sampleSumXcm += fix.ecefXcm;
    m_sampleSumYcm += fix.ecefYcm;
    m_sampleSumZcm += fix.ecefZcm;
    ++m_sampleCount;

    if (m_sampleCount >= kMaxVerifySamples) {
        m_verifyTimeout.stop();
        finishVerification();
    }
}

void TimeModeSupervisor::onVerifyTimeout() {
    if (m_state == State::CollectingVerificationSamples)
        finishVerification();
}

void TimeModeSupervisor::finishVerification() {
    if (m_state != State::CollectingVerificationSamples)
        return;
    m_state = State::Idle;

    if (m_sampleCount < kMinVerifySamples) {
        QTextStream(stderr) << "TimeModeSupervisor: te weinig verse posities ontvangen ("
                             << m_sampleCount << "/" << kMinVerifySamples
                             << " nodig binnen " << (kVerifyTimeoutMs / 1000)
                             << "s) -- val veilig terug op de opgeslagen positie zonder te "
                                "beoordelen of er verplaatst is.\n";
        m_gpsLink->setFixedPosition(m_verifyAgainstXcm, m_verifyAgainstYcm, m_verifyAgainstZcm,
                                     m_verifyAgainstVarMm2);
        return;
    }

    const double avgXcm = static_cast<double>(m_sampleSumXcm) / m_sampleCount;
    const double avgYcm = static_cast<double>(m_sampleSumYcm) / m_sampleCount;
    const double avgZcm = static_cast<double>(m_sampleSumZcm) / m_sampleCount;

    const double dxM = (avgXcm - m_verifyAgainstXcm) / 100.0;
    const double dyM = (avgYcm - m_verifyAgainstYcm) / 100.0;
    const double dzM = (avgZcm - m_verifyAgainstZcm) / 100.0;
    const double distanceM = std::sqrt(dxM * dxM + dyM * dyM + dzM * dzM);

    QTextStream(stderr) << "TimeModeSupervisor: " << m_sampleCount << " verse posities gemiddeld, "
                         << "afstand t.o.v. opgeslagen referentie: " << distanceM << " m "
                         << "(drempel " << m_moveThresholdMeters << " m).\n";

    if (distanceM < m_moveThresholdMeters) {
        QTextStream(stderr) << "TimeModeSupervisor: geen (relevante) verplaatsing -- "
                                "Fixed Mode hersteld met de bekende, nauwkeurige positie.\n";
        m_gpsLink->setFixedPosition(m_verifyAgainstXcm, m_verifyAgainstYcm, m_verifyAgainstZcm,
                                     m_verifyAgainstVarMm2);
    } else {
        QTextStream(stderr) << "TimeModeSupervisor: antenne lijkt verplaatst -- start een "
                                "verse Survey-In (min " << m_minDurationSeconds << "s).\n";
        m_gpsLink->startSurveyIn(m_minDurationSeconds, m_varLimitMm2);
        // Resultaat wordt via onSurveyInUpdated() opgeslagen zodra de nieuwe
        // Survey-In voltooit (valid && !active).
    }
}

void TimeModeSupervisor::onSurveyInUpdated(const SurveyInStatus &status) {
    if (!status.valid || status.active)
        return; // alleen een daadwerkelijk VOLTOOIDE Survey-In is een nieuwe referentie
    savePersistedPosition(status.meanXcm, status.meanYcm, status.meanZcm, status.meanVarMm2);
}

QString TimeModeSupervisor::stateFilePath() {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QStringLiteral("/gps_fixed_position.json");
}

bool TimeModeSupervisor::loadPersistedPosition(qint32 &xcm, qint32 &ycm, qint32 &zcm, quint32 &varMm2) const {
    QFile file(stateFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;
    const QJsonObject obj = doc.object();
    if (!obj.contains(QStringLiteral("ecefXcm")) || !obj.contains(QStringLiteral("ecefYcm"))
        || !obj.contains(QStringLiteral("ecefZcm")))
        return false;
    xcm = obj.value(QStringLiteral("ecefXcm")).toInt();
    ycm = obj.value(QStringLiteral("ecefYcm")).toInt();
    zcm = obj.value(QStringLiteral("ecefZcm")).toInt();
    varMm2 = static_cast<quint32>(obj.value(QStringLiteral("posVarMm2")).toDouble());
    return true;
}

void TimeModeSupervisor::savePersistedPosition(qint32 xcm, qint32 ycm, qint32 zcm, quint32 varMm2) {
    if (m_lastWrittenKnown && xcm == m_lastWrittenXcm && ycm == m_lastWrittenYcm
        && zcm == m_lastWrittenZcm && varMm2 == m_lastWrittenVarMm2)
        return; // ongewijzigd -- geen nodeloze SD-kaart-schrijfactie

    QJsonObject obj;
    obj[QStringLiteral("ecefXcm")] = xcm;
    obj[QStringLiteral("ecefYcm")] = ycm;
    obj[QStringLiteral("ecefZcm")] = zcm;
    obj[QStringLiteral("posVarMm2")] = static_cast<double>(varMm2);

    QFile file(stateFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream(stderr) << "TimeModeSupervisor: kon referentiepositie niet opslaan naar "
                             << file.fileName() << "\n";
        return;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));

    m_lastWrittenKnown = true;
    m_lastWrittenXcm = xcm;
    m_lastWrittenYcm = ycm;
    m_lastWrittenZcm = zcm;
    m_lastWrittenVarMm2 = varMm2;
    m_persistedFileConfirmed = true;
}
