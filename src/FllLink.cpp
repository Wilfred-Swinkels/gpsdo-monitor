// FllLink.cpp
//
// Bronvermelding (VE2ZAZ GPS_Std_UserMan_v1.1.pdf):
//  - Sectie 4.3 "Power up": na power-up laadt de firmware de laatst opgeslagen
//    parameters uit EEPROM en start ALTIJD in Unlocked, waarna hij automatisch
//    begint met samples verzamelen. Er is dus geen "start commando" nodig — wij
//    grijpen alleen in op de S/M-parameters om die opstart te versnellen.
//  - Sectie 4.4.3.1 (Sample Summing): "This mode is more effective when the
//    frequency averaging cycle size 'S' is smaller ... This mode is helpful in
//    acquiring a lock at a faster pace."
//  - Sectie 4.4.3.2 (Sample Voting): "... is more effective when the frequency
//    averaging cycle size 'S' is large ... Tests have also shown that this mode
//    yields the best accuracy. The Sample Voting mode should be used in
//    conjunction with long frequency averaging cycles."
//  - Sectie 4.4.5 ("S" FLL Parameter Setting): "During initial setup, the user
//    may want to reduce the Average Sample Size 'S' parameter to accelerate
//    frequency acquisition. A value of 0x000A (10 samples) is suggested. Once
//    the system is in Locked state and stabilized, the user may increase S to
//    a larger value ... a cycle size 'S' of 0x0200 (512 samples) yielded the
//    expected accuracy with the Garmin GPS-35 receiver. This was accomplished
//    in Sample Voting mode."
//  - Sectie 4.4.4 ("N","L","H","F" parameters): expliciet "different values may
//    yield better FLL stability depending on the GPS receiving unit or the
//    VCXO used. The user will want to experiment with these" — daarom raakt
//    deze code N/L/H/F bewust niet aan.
//  - Sectie 6 (Serial Port User Commands): 4800 baud 8N1 geen flow control,
//    commando's in hoofdletters, afgesloten met <CR><LF>, hex-waarde 0x00 is
//    nergens toegestaan als parameter.
//
// Wat deze code NIET doet (en waarom):
//  - Geen automatische WWV/CHU zero-beat voor-tunen (handleiding 4.4.2). Dat is
//    een fysieke, eenmalige bank-procedure met een HF-ontvanger — geen firmware.
//    Bovendien voedt de LPRO-101 hier vermoedelijk al een zeer stabiele ~1-2e-10
//    referentie, dus de initiële DAC-fout is hoogstwaarschijnlijk al klein; dit
//    is de grootste "extra" hendel als de acquisitie in de praktijk toch traag
//    blijkt, niet iets om nu te automatiseren.
//  - Geen giswerk aan N/L/H/F: die blijven op de firmware-default totdat ze met
//    de echte GPS/VCXO-combinatie empirisch afgesteld zijn.

#include "FllLink.h"
#include <QDebug>

FllLink::FllLink(QObject *parent) : QObject(parent) {
    connect(&m_port, &QSerialPort::readyRead, this, &FllLink::onReadyRead);
}

bool FllLink::open(const QString &portName) {
    m_port.setPortName(portName);
    m_port.setBaudRate(4800);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        qWarning() << "FllLink: kon" << portName << "niet openen:" << m_port.errorString();
        return false;
    }

    // Bewust GEEN commando's sturen vóórdat we weten in welke staat het
    // apparaat al verkeert (zie sectie 4.3: het start vanzelf, er is geen
    // "aanzet"-commando nodig). Als we hier blind de acquisitiefase zouden
    // starten, verstoren we een apparaat dat al gelockt en gestabiliseerd
    // is onnodig terug naar de snelle/minder-nauwkeurige M02/S000A-modus —
    // dat kost dan ~13 minuten (5 cycli × 160s) om weer terug te schakelen
    // naar steady-state. Zie handleLine(): de eerste ontvangen statusregel
    // bepaalt of we de acquisitiefase starten of meteen steady-state aannemen.
    m_haveSeenFirstStatus = false;
    return true;
}

void FllLink::close() {
    m_port.close();
}

void FllLink::sendCommand(const QString &cmd) {
    // Handleiding sectie 6: hoofdletters, afgesloten met <CR><LF>.
    m_port.write((cmd + QStringLiteral("\r\n")).toLatin1());
}

void FllLink::beginAcquisitionPhase() {
    m_phase = Phase::Acquisition;
    m_consecutiveLocked = 0;

    // Zorg dat de FLL-regeling actief is (voor het geval een vorige sessie
    // 'm in Disabled state achterliet, bv. na handmatig tunen — 4.4.2 stap 10).
    sendCommand(QStringLiteral("E"));
    // Begin met een schone alarm-latch zodat de statusstring niet meteen een
    // oude, allang niet meer relevante alarmvlag toont.
    sendCommand(QStringLiteral("A"));

    // De eigenlijke snelheids-optimalisatie: kleine averaging-cyclus + Summing.
    sendCommand(QString::fromLatin1(kAcquisitionMode));
    sendCommand(QString::fromLatin1(kAcquisitionCycle));
}

void FllLink::switchToSteadyStatePhase() {
    if (m_phase == Phase::SteadyState)
        return;
    m_phase = Phase::SteadyState;

    // Lock bevestigd: nu omschakelen naar de configuratie die de handleiding
    // zelf aanwijst als beste-nauwkeurigheid-op-lange-termijn.
    sendCommand(QString::fromLatin1(kSteadyStateMode));
    sendCommand(QString::fromLatin1(kSteadyStateCycle));
}

void FllLink::onReadyRead() {
    m_rxBuffer += m_port.readAll();

    int idx;
    while ((idx = m_rxBuffer.indexOf("\r\n")) != -1) {
        const QByteArray line = m_rxBuffer.left(idx);
        m_rxBuffer.remove(0, idx + 2);
        if (!line.isEmpty())
            handleLine(line);
    }
}

void FllLink::handleLine(const QByteArray &line) {
    emit rawLineReceived(line);

    FllStatus status;
    QString parseErrorReason;
    // Alleen de 10-velden FLL-statusstring wordt hier verwerkt (sectie 5.1).
    // De parameterstring (sectie 5.2, andere vorm) en menu-/versietekst
    // worden door parseStatus() vanzelf afgewezen (verkeerd aantal velden) —
    // een volledige implementatie zou die apart parsen om S/M-bevestigingen
    // te controleren, maar dat is niet nodig voor de lock-versnellingslogica
    // zelf. We laten dit WEL via parseError() weten i.p.v. stilletjes te
    // negeren, zodat "er komt niets binnen" en "er komt iets binnen maar het
    // parseert niet" van elkaar te onderscheiden zijn.
    if (!parseStatus(line, status, parseErrorReason)) {
        emit parseError(line, parseErrorReason);
        return;
    }

    emit statusUpdated(status);

    if (!m_haveSeenFirstStatus) {
        m_haveSeenFirstStatus = true;
        if (status.state == QLatin1Char('L')) {
            // Al gelockt bij het verbinden: niet onnodig verstoren met de
            // acquisitiefase. Meteen als bevestigd beschouwen en naar de
            // nauwkeurige steady-state-configuratie (M01/S0200) — we weten
            // niet zeker welke M/S het apparaat op dit moment gebruikt
            // (kan nog de vorige sessie zijn), dus expliciet zetten i.p.v.
            // aannemen dat het al goed staat.
            m_phase = Phase::Acquisition; // switchToSteadyStatePhase() verwacht dit
            m_consecutiveLocked = kLockConfirmCycles;
            m_haveAcquiredLock = true;
            emit lockAcquired();
            switchToSteadyStatePhase();
        } else {
            // Nog niet gelockt: dit is precies het geval waarvoor de
            // snel-lock-strategie bedoeld is.
            beginAcquisitionPhase();
        }
        return;
    }

    if (status.state == QLatin1Char('L')) {
        if (++m_consecutiveLocked >= kLockConfirmCycles) {
            if (!m_haveAcquiredLock) {
                m_haveAcquiredLock = true;
                emit lockAcquired();
                switchToSteadyStatePhase();
            }
        }
    } else {
        if (m_haveAcquiredLock && m_consecutiveLocked >= kLockConfirmCycles) {
            emit lockLost();
        }
        m_consecutiveLocked = 0;
    }
}

bool FllLink::parseStatus(const QByteArray &line, FllStatus &out, QString &errorOut) {
    // Voorbeeld: L | U | 01FF6 | + | F | 67FC | 0120 | FFFD | 007D | 03
    const QList<QByteArray> f = line.split('|');
    if (f.size() != 10) {
        errorOut = QStringLiteral("verwacht 10 met '|' gescheiden velden, kreeg er %1").arg(f.size());
        return false;
    }

    auto trimmed = [](const QByteArray &b) { return b.trimmed(); };
    bool ok = true;

    const QByteArray stateF = trimmed(f[0]);
    if (stateF.size() != 1) { errorOut = QStringLiteral("veld 1 (state) is geen 1 teken"); return false; }
    out.state = QChar::fromLatin1(stateF.at(0));

    const QByteArray alarmF = trimmed(f[1]);
    if (alarmF.size() != 1) { errorOut = QStringLiteral("veld 2 (alarm) is geen 1 teken"); return false; }
    out.alarmLatch = QChar::fromLatin1(alarmF.at(0));

    out.dacValue = trimmed(f[2]).toUShort(&ok, 16);
    if (!ok) { errorOut = QStringLiteral("veld 3 (dac) is geen geldige hex-waarde"); return false; }

    const QByteArray signF = trimmed(f[3]);
    if (signF.size() != 1) { errorOut = QStringLiteral("veld 4 (freq.sign) is geen 1 teken"); return false; }
    out.freqAdjSign = QChar::fromLatin1(signF.at(0));

    const QByteArray sizeF = trimmed(f[4]);
    if (sizeF.size() != 1) { errorOut = QStringLiteral("veld 5 (freq.size) is geen 1 teken"); return false; }
    out.freqAdjSize = QChar::fromLatin1(sizeF.at(0));

    out.freqReadout = trimmed(f[5]).toUShort(&ok, 16);
    if (!ok) { errorOut = QStringLiteral("veld 6 (freq-uitlezing) is geen geldige hex-waarde"); return false; }
    out.sampleCounter = trimmed(f[6]).toUShort(&ok, 16);
    if (!ok) { errorOut = QStringLiteral("veld 7 (sample-teller) is geen geldige hex-waarde"); return false; }
    out.accumFreqDiff = static_cast<qint16>(trimmed(f[7]).toUShort(&ok, 16));
    if (!ok) { errorOut = QStringLiteral("veld 8 (accum.diff) is geen geldige hex-waarde"); return false; }
    out.timestamp = trimmed(f[8]).toUShort(&ok, 16);
    if (!ok) { errorOut = QStringLiteral("veld 9 (timestamp) is geen geldige hex-waarde"); return false; }
    out.holdoverCounter = trimmed(f[9]).toUShort(&ok, 16);
    if (!ok) { errorOut = QStringLiteral("veld 10 (holdover) is geen geldige hex-waarde"); return false; }

    out.valid = true;
    return true;
}
