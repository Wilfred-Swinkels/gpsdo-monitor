// Mcp3426Adc.cpp
//
// Zie Mcp3426Adc.h voor de volledige bronvermelding en registerlayout.
//
// I2C op Linux/Raspberry Pi loopt hier via de kale i2c-dev-interface
// (open() + ioctl(I2C_SLAVE) + write()/read() op /dev/i2c-N), niet via een
// Qt-klasse — Qt heeft geen ingebouwde I2C-ondersteuning. Dat betekent
// blokkerende systeemcalls, en dat is precies waarom deze klasse zijn eigen
// QThread heeft: de I2C-polling mag de QML/UI-thread nooit raken.

#include "Mcp3426Adc.h"

#include <QDebug>

#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>

Mcp3426Adc::Mcp3426Adc(QObject *parent) : QObject(parent) {
    moveToThread(&m_thread);
    connect(&m_thread, &QThread::started, this, &Mcp3426Adc::init);
}

Mcp3426Adc::~Mcp3426Adc() {
    stop();
}

void Mcp3426Adc::start(const QString &i2cDevice, quint8 i2cAddress,
                        const ChannelConfig &ch1, const ChannelConfig &ch2,
                        int pollIntervalMs) {
    m_i2cDevice = i2cDevice;
    m_i2cAddress = i2cAddress;
    m_ch1 = ch1;
    m_ch2 = ch2;
    m_pollIntervalMs = pollIntervalMs;
    m_thread.start();
}

void Mcp3426Adc::stop() {
    if (!m_thread.isRunning())
        return;
    m_thread.quit();
    m_thread.wait();
    if (m_fd >= 0) {
        ::close(m_fd);
        m_fd = -1;
    }
}

void Mcp3426Adc::init() {
    // Draait al op m_thread (aangeroepen via QThread::started).
    m_fd = ::open(m_i2cDevice.toLocal8Bit().constData(), O_RDWR);
    if (m_fd < 0) {
        emit errorOccurred(QStringLiteral("kon %1 niet openen: %2")
                                .arg(m_i2cDevice, QString::fromLocal8Bit(std::strerror(errno))));
        return;
    }

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Mcp3426Adc::pollOnce);
    m_timer->start(m_pollIntervalMs);

    // Meteen één ronde doen i.p.v. te wachten op de eerste timer-tick, zodat
    // de eerste waarden er niet pas na pollIntervalMs zijn.
    pollOnce();
}

void Mcp3426Adc::pollOnce() {
    qint16 raw = 0;

    if (readChannel(m_ch1, raw)) {
        const double adcVolts = (raw / 32768.0) * kVref; // 1x gain, ±2.048V volle schaal
        emit voltageRead(m_ch1.name, adcVolts * m_ch1.dividerRatio, raw);
    }
    if (readChannel(m_ch2, raw)) {
        const double adcVolts = (raw / 32768.0) * kVref;
        emit voltageRead(m_ch2.name, adcVolts * m_ch2.dividerRatio, raw);
    }
}

bool Mcp3426Adc::readChannel(const ChannelConfig &cfg, qint16 &rawCodeOut) {
    if (m_fd < 0)
        return false;

    if (ioctl(m_fd, I2C_SLAVE, m_i2cAddress) < 0) {
        emit errorOccurred(QStringLiteral("I2C_SLAVE ioctl faalde voor adres 0x%1: %2")
                                .arg(m_i2cAddress, 2, 16, QLatin1Char('0'))
                                .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    const quint8 channelBits = (cfg.channel == 2) ? kChannel2Bits : kChannel1Bits;
    const quint8 configByte = kStartConversion | channelBits | kOneShotMode
                               | kResolution16Bit | kGain1x;

    if (!writeConfig(configByte))
        return false;

    // Conversietijd afwachten vóór de eerste leespoging (zie kConversionTimeMs).
    QThread::msleep(kConversionTimeMs);

    for (int attempt = 0; attempt < kMaxPollAttempts; ++attempt) {
        quint8 configEcho = 0;
        qint16 raw = 0;
        if (!readResult(configEcho, raw))
            return false; // readResult heeft zelf al errorOccurred ge-emit

        if ((configEcho & kNotReadyMask) == 0) {
            // RDY-bit is 0: conversie klaar, dit is een verse sample.
            rawCodeOut = raw;
            return true;
        }
        QThread::msleep(kPollRetryDelayMs);
    }

    emit errorOccurred(QStringLiteral("MCP3426 kanaal %1 ('%2'): geen conversie klaar na %3ms — "
                                       "bekabeling/adres controleren")
                            .arg(cfg.channel)
                            .arg(cfg.name)
                            .arg(kConversionTimeMs + kMaxPollAttempts * kPollRetryDelayMs));
    return false;
}

bool Mcp3426Adc::writeConfig(quint8 configByte) {
    if (::write(m_fd, &configByte, 1) != 1) {
        emit errorOccurred(QStringLiteral("I2C-write naar MCP3426 faalde: %1")
                                .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }
    return true;
}

bool Mcp3426Adc::readResult(quint8 &configByteOut, qint16 &rawCodeOut) {
    // 3 bytes: 2 databytes (MSB eerst, sign-extended two's complement over
    // de volle 16 bit) + het actuele configuratiebyte (bevat de RDY-status).
    quint8 buf[3] = {0, 0, 0};
    if (::read(m_fd, buf, sizeof(buf)) != static_cast<ssize_t>(sizeof(buf))) {
        emit errorOccurred(QStringLiteral("I2C-read van MCP3426 faalde: %1")
                                .arg(QString::fromLocal8Bit(std::strerror(errno))));
        return false;
    }

    configByteOut = buf[2];
    rawCodeOut = static_cast<qint16>((static_cast<quint16>(buf[0]) << 8) | buf[1]);
    return true;
}
