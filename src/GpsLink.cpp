// GpsLink.cpp
//
// Zie GpsLink.h voor de volledige bronvermelding en byte-layout van de
// gebruikte UBX-berichten (NAV-SVINFO, NAV-SOL, NAV-DOP).

#include "GpsLink.h"

#include <QtEndian>
#include <QDebug>

namespace {
constexpr quint8 kSync1 = 0xB5;
constexpr quint8 kSync2 = 0x62;
constexpr quint8 kClassNav = 0x01;
constexpr quint8 kIdNavSol = 0x06;
constexpr quint8 kIdNavDop = 0x04;
constexpr quint8 kIdNavSvinfo = 0x30;
constexpr quint8 kIdNavPosllh = 0x02;
constexpr quint8 kClassCfg = 0x06;
constexpr quint8 kIdCfgMsg = 0x01;
constexpr quint8 kIdCfgTmode = 0x1D;
constexpr quint8 kClassTim = 0x0D;
constexpr quint8 kIdTimSvin = 0x04;

// Kleine helpers om little-endian 32-bit velden in een CFG-payload op te
// bouwen — de rest van dit bestand doet alleen het omgekeerde (uitlezen via
// qFromLittleEndian), dit is de eerste plek die zelf een multi-byte veld
// moet SCHRIJVEN (CFG-MSG hierboven is toevallig altijd 1-byte velden).
void appendU4(QByteArray &buf, quint32 v) {
    buf.append(static_cast<char>(v & 0xFF));
    buf.append(static_cast<char>((v >> 8) & 0xFF));
    buf.append(static_cast<char>((v >> 16) & 0xFF));
    buf.append(static_cast<char>((v >> 24) & 0xFF));
}
void appendI4(QByteArray &buf, qint32 v) {
    appendU4(buf, static_cast<quint32>(v));
}
} // namespace

GpsLink::GpsLink(QObject *parent) : QObject(parent) {
    connect(&m_port, &QSerialPort::readyRead, this, &GpsLink::onReadyRead);
}

bool GpsLink::open(const QString &portName) {
    m_port.setPortName(portName);
    m_port.setBaudRate(9600);
    m_port.setDataBits(QSerialPort::Data8);
    m_port.setParity(QSerialPort::NoParity);
    m_port.setStopBits(QSerialPort::OneStop);
    m_port.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_port.open(QIODevice::ReadWrite)) {
        qWarning() << "GpsLink: kon" << portName << "niet openen:" << m_port.errorString();
        return false;
    }

    m_rxBuffer.clear();
    m_lastFix = GpsFix{};
    enablePeriodicMessages();
    return true;
}

void GpsLink::close() {
    m_port.close();
}

bool GpsLink::startSurveyIn(quint32 minDurationSeconds, quint32 varLimitMm2) {
    if (!m_port.isOpen())
        return false;
    // CFG-TMODE payload (28 bytes) — zie GpsLink.h voor de bronvermelding.
    // fixedPosX/Y/Z/fixedPosVar zijn ongebruikt in Survey-In-mode (die zijn
    // alleen relevant bij timeMode=2/Fixed), dus gewoon op 0.
    QByteArray payload;
    appendU4(payload, 1); // timeMode = 1 (Survey In)
    appendI4(payload, 0); // fixedPosX
    appendI4(payload, 0); // fixedPosY
    appendI4(payload, 0); // fixedPosZ
    appendU4(payload, 0); // fixedPosVar
    appendU4(payload, minDurationSeconds);
    appendU4(payload, varLimitMm2);
    m_port.write(buildFrame(kClassCfg, kIdCfgTmode, payload));
    return true;
}

bool GpsLink::disableTimeMode() {
    if (!m_port.isOpen())
        return false;
    QByteArray payload;
    appendU4(payload, 0); // timeMode = 0 (Disabled)
    appendI4(payload, 0);
    appendI4(payload, 0);
    appendI4(payload, 0);
    appendU4(payload, 0);
    appendU4(payload, 0);
    appendU4(payload, 0);
    m_port.write(buildFrame(kClassCfg, kIdCfgTmode, payload));
    return true;
}

bool GpsLink::setFixedPosition(qint32 ecefXcm, qint32 ecefYcm, qint32 ecefZcm, quint32 posVarMm2) {
    if (!m_port.isOpen())
        return false;
    QByteArray payload;
    appendU4(payload, 2); // timeMode = 2 (Fixed)
    appendI4(payload, ecefXcm);
    appendI4(payload, ecefYcm);
    appendI4(payload, ecefZcm);
    appendU4(payload, posVarMm2);
    appendU4(payload, 0); // svinMinDur — ongebruikt in Fixed Mode
    appendU4(payload, 0); // svinVarLimit — ongebruikt in Fixed Mode
    m_port.write(buildFrame(kClassCfg, kIdCfgTmode, payload));
    return true;
}

bool GpsLink::requestAutoSurveyIn(quint32 minDurationSeconds, quint32 varLimitMm2) {
    if (!m_port.isOpen())
        return false;
    m_autoSurveyInPending = true;
    m_autoSurveyMinDurationSeconds = minDurationSeconds;
    m_autoSurveyVarLimitMm2 = varLimitMm2;
    // CFG-TMODE Poll Request: lege payload -- de module antwoordt zelf met
    // een CFG-TMODE-bericht (28 bytes) met de HUIDIGE instelling. Zie
    // handleCfgTmodeResponse() voor wat daarmee gebeurt.
    m_port.write(buildFrame(kClassCfg, kIdCfgTmode, QByteArray()));
    return true;
}

void GpsLink::enablePeriodicMessages() {
    // CFG-MSG met een 3-byte payload (class, id, rate) zet de output-rate
    // van dat bericht op de poort waarop het commando binnenkomt — hier dus
    // UART1. rate=1 betekent: 1x per navigatie-epoche (standaard 1Hz op
    // deze module). RAM-only, wordt niet weggeschreven naar flash/BBR — zie
    // toelichting in GpsLink.h.
    const QList<QPair<quint8, quint8>> toEnable = {
        {kClassNav, kIdNavSvinfo},
        {kClassNav, kIdNavSol},
        {kClassNav, kIdNavDop},
        {kClassNav, kIdNavPosllh},
        {kClassTim, kIdTimSvin},
    };
    for (const auto &msg : toEnable) {
        QByteArray payload;
        payload.append(static_cast<char>(msg.first));
        payload.append(static_cast<char>(msg.second));
        payload.append(static_cast<char>(1)); // rate
        m_port.write(buildFrame(kClassCfg, kIdCfgMsg, payload));
    }
}

QByteArray GpsLink::buildFrame(quint8 msgClass, quint8 msgId, const QByteArray &payload) {
    QByteArray frame;
    frame.append(static_cast<char>(kSync1));
    frame.append(static_cast<char>(kSync2));
    frame.append(static_cast<char>(msgClass));
    frame.append(static_cast<char>(msgId));
    const quint16 len = static_cast<quint16>(payload.size());
    frame.append(static_cast<char>(len & 0xFF));
    frame.append(static_cast<char>((len >> 8) & 0xFF));
    frame.append(payload);

    // Checksum over alles NA de 2 syncbytes (dus class t/m payload).
    quint8 ckA = 0, ckB = 0;
    for (int i = 2; i < frame.size(); ++i) {
        ckA = static_cast<quint8>(ckA + static_cast<quint8>(frame[i]));
        ckB = static_cast<quint8>(ckB + ckA);
    }
    frame.append(static_cast<char>(ckA));
    frame.append(static_cast<char>(ckB));
    return frame;
}

void GpsLink::onReadyRead() {
    m_rxBuffer += m_port.readAll();
    processBuffer();
}

void GpsLink::processBuffer() {
    while (true) {
        const int syncIdx = m_rxBuffer.indexOf(QByteArray("\xB5\x62", 2));
        if (syncIdx < 0) {
            // Geen volledige sync gevonden. Bewaar hooguit de laatste byte,
            // voor het geval dat een losse 0xB5 het begin is van een sync
            // die in de volgende leesbeurt compleet wordt (NMEA-tekst is
            // 7-bit ASCII en kan 0xB5 sowieso nooit bevatten, dus dit is
            // altijd ofwel een echte aankomende sync ofwel ruis).
            if (!m_rxBuffer.isEmpty() && static_cast<quint8>(m_rxBuffer.back()) == kSync1)
                m_rxBuffer = m_rxBuffer.right(1);
            else
                m_rxBuffer.clear();
            return;
        }
        if (syncIdx > 0)
            m_rxBuffer.remove(0, syncIdx);

        if (m_rxBuffer.size() < 6)
            return; // wachten op class/id/length

        const quint8 msgClass = static_cast<quint8>(m_rxBuffer[2]);
        const quint8 msgId = static_cast<quint8>(m_rxBuffer[3]);
        const quint16 payloadLen = qFromLittleEndian<quint16>(
            reinterpret_cast<const uchar *>(m_rxBuffer.constData() + 4));

        if (payloadLen > kMaxPlausiblePayload) {
            // Onwaarschijnlijk grote lengte: waarschijnlijk toevallige
            // 0xB5 0x62-bytes binnen een payload i.p.v. een echte sync.
            // Sla alleen deze 2 syncbytes over en zoek verder, i.p.v. voor
            // altijd op nooit-komende data te blijven wachten.
            m_rxBuffer.remove(0, 2);
            continue;
        }

        const int frameLen = 6 + payloadLen + 2;
        if (m_rxBuffer.size() < frameLen)
            return; // wachten op de rest van dit bericht

        quint8 ckA = 0, ckB = 0;
        for (int i = 2; i < 6 + payloadLen; ++i) {
            ckA = static_cast<quint8>(ckA + static_cast<quint8>(m_rxBuffer[i]));
            ckB = static_cast<quint8>(ckB + ckA);
        }
        const quint8 gotCkA = static_cast<quint8>(m_rxBuffer[6 + payloadLen]);
        const quint8 gotCkB = static_cast<quint8>(m_rxBuffer[6 + payloadLen + 1]);

        if (ckA == gotCkA && ckB == gotCkB) {
            dispatch(msgClass, msgId, m_rxBuffer.mid(6, payloadLen));
        } else {
            emit errorOccurred(QStringLiteral("UBX-checksum-fout (class=0x%1 id=0x%2) — bericht genegeerd")
                                    .arg(msgClass, 2, 16, QLatin1Char('0'))
                                    .arg(msgId, 2, 16, QLatin1Char('0')));
        }

        m_rxBuffer.remove(0, frameLen);
        // Loop verder: er kan meteen nog een volledig bericht in de buffer zitten.
    }
}

void GpsLink::dispatch(quint8 msgClass, quint8 msgId, const QByteArray &payload) {
    if (msgClass == kClassNav && msgId == kIdNavSvinfo) {
        handleNavSvinfo(payload);
    } else if (msgClass == kClassNav && msgId == kIdNavSol) {
        handleNavSol(payload);
    } else if (msgClass == kClassNav && msgId == kIdNavDop) {
        handleNavDop(payload);
    } else if (msgClass == kClassNav && msgId == kIdNavPosllh) {
        handleNavPosllh(payload);
    } else if (msgClass == kClassTim && msgId == kIdTimSvin) {
        handleTimSvin(payload);
    } else if (msgClass == kClassCfg && msgId == kIdCfgTmode) {
        // Komt hier alleen binnen als antwoord op onze eigen poll request
        // uit requestAutoSurveyIn() -- de module stuurt dit niet ongevraagd.
        handleCfgTmodeResponse(payload);
    }
    // Andere klasse/id-combinaties (bv. ACK-ACK op onze CFG-MSG-commando's)
    // worden bewust genegeerd — dit is geen generieke UBX-bibliotheek, alleen
    // wat dit project nodig heeft.
}

void GpsLink::handleNavSvinfo(const QByteArray &payload) {
    if (payload.size() < 8)
        return;
    const quint8 numCh = static_cast<quint8>(payload[4]);
    const int expected = 8 + static_cast<int>(numCh) * 12;
    if (payload.size() < expected)
        return;

    QList<GpsSatellite> sats;
    sats.reserve(numCh);
    for (int i = 0; i < numCh; ++i) {
        const int off = 8 + i * 12;
        GpsSatellite sat;
        sat.channel = static_cast<quint8>(payload[off + 0]);
        sat.svid = static_cast<quint8>(payload[off + 1]);
        const quint8 flags = static_cast<quint8>(payload[off + 2]);
        sat.usedInFix = (flags & 0x01) != 0;
        sat.unhealthy = (flags & 0x10) != 0;
        sat.cno = static_cast<quint8>(payload[off + 4]);
        sat.elevationDeg = static_cast<qint8>(payload[off + 5]);
        sat.azimuthDeg = qFromLittleEndian<qint16>(
            reinterpret_cast<const uchar *>(payload.constData() + off + 6));
        sats.append(sat);
    }
    emit satellitesUpdated(sats);
}

void GpsLink::handleNavSol(const QByteArray &payload) {
    if (payload.size() < 52)
        return;

    GpsFix fix = m_lastFix; // hdop komt uit NAV-DOP, hier behouden
    fix.fixType = static_cast<quint8>(payload[10]);
    const quint8 flags = static_cast<quint8>(payload[11]);
    fix.fixOk = (flags & 0x01) != 0;
    fix.ecefXcm = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 12));
    fix.ecefYcm = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 16));
    fix.ecefZcm = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 20));
    fix.hasEcef = true;
    fix.pdop = qFromLittleEndian<quint16>(
                   reinterpret_cast<const uchar *>(payload.constData() + 44)) / 100.0;
    fix.numSatellites = static_cast<quint8>(payload[47]);
    fix.valid = true;

    m_lastFix = fix;
    emit fixUpdated(m_lastFix);
}

void GpsLink::handleNavDop(const QByteArray &payload) {
    if (payload.size() < 18)
        return;

    GpsFix fix = m_lastFix; // fixType/numSatellites/pdop komen uit NAV-SOL
    fix.hdop = qFromLittleEndian<quint16>(
                   reinterpret_cast<const uchar *>(payload.constData() + 12)) / 100.0;
    fix.valid = true;

    m_lastFix = fix;
    emit fixUpdated(m_lastFix);
}

void GpsLink::handleNavPosllh(const QByteArray &payload) {
    if (payload.size() < 28)
        return;

    GpsFix fix = m_lastFix; // fixType/hdop/etc. komen uit NAV-SOL/NAV-DOP
    const qint32 lon = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 4));
    const qint32 lat = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 8));
    fix.longitudeDeg = lon / 1.0e7;
    fix.latitudeDeg = lat / 1.0e7;
    fix.hasPosition = true;
    fix.valid = true;

    m_lastFix = fix;
    emit fixUpdated(m_lastFix);
}

void GpsLink::handleTimSvin(const QByteArray &payload) {
    if (payload.size() < 28)
        return;

    SurveyInStatus s;
    s.durationSec = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 0));
    s.meanXcm = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 4));
    s.meanYcm = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 8));
    s.meanZcm = qFromLittleEndian<qint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 12));
    s.meanVarMm2 = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 16));
    s.observations = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(payload.constData() + 20));
    s.valid = static_cast<quint8>(payload[24]) != 0;
    s.active = static_cast<quint8>(payload[25]) != 0;

    emit surveyInUpdated(s);
}

void GpsLink::handleCfgTmodeResponse(const QByteArray &payload) {
    if (payload.size() < 4)
        return; // te kort om zelfs maar timeMode uit te lezen -- negeren

    const quint32 timeMode = qFromLittleEndian<quint32>(
        reinterpret_cast<const uchar *>(payload.constData()));
    emit timeModeReported(static_cast<quint8>(timeMode));

    if (!m_autoSurveyInPending)
        return;
    m_autoSurveyInPending = false; // eenmalig -- zie header-toelichting

    if (timeMode == 0) {
        // Nog Disabled: dit is de eerste keer voor deze module (of de
        // module is stroomloos geweest sinds de vorige keer) -- nu pas
        // echt Survey-In starten.
        startSurveyIn(m_autoSurveyMinDurationSeconds, m_autoSurveyVarLimitMm2);
    }
    // timeMode == 1 (Survey-In loopt al) of == 2 (al Fixed): bewust NIETS
    // doen -- dat bestaande resultaat blijft met rust. Zie GpsLink.h voor
    // waarom dit geen "is de antenne verplaatst?"-detectie doet.
}
