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
constexpr quint8 kClassCfg = 0x06;
constexpr quint8 kIdCfgMsg = 0x01;
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
