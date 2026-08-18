// BiteLockDriver.cpp
//
// Zie BiteLockDriver.h voor de volledige uitleg. Net als Tsic506Driver een
// eigen QThread (zodat de QObject niet op de aanroepende/UI-thread hoeft te
// leven — de pigpio-callback zelf draait sowieso op zijn eigen thread,
// onafhankelijk van m_thread).

#include "BiteLockDriver.h"
#include "PigpioGuard.h"

#include <QDebug>

#include <pigpio.h>

BiteLockDriver::BiteLockDriver(QObject *parent) : QObject(parent) {
    moveToThread(&m_thread);
    m_thread.start();
}

BiteLockDriver::~BiteLockDriver() {
    stop();
}

void BiteLockDriver::start(int gpioPin) {
    m_gpioPin = gpioPin;

    // Gedeelde, ref-getelde pigpio-init — zie PigpioGuard.h. Veilig samen
    // met Tsic506Driver, die in hetzelfde proces ook pigpio gebruikt.
    if (!PigpioGuard::acquire()) {
        emit errorOccurred(QStringLiteral(
            "pigpio-initialisatie faalde — draait dit proces als root, en is "
            "de pigpiod-daemon (indien geïnstalleerd) uitgeschakeld?"));
        return;
    }
    m_pigpioReady = true;

    gpioSetMode(m_gpioPin, PI_INPUT);

    // Zelfde DMA-gebaseerde edge-callback als Tsic506Driver (GEEN
    // gpioSetISRFuncEx() — die gebruikt het op deze Pi/kernel kapotte
    // sysfs-GPIO-mechanisme, zie Tsic506Driver.h/de projectbrief) — hier
    // puur voor niveauwissel-detectie, geen bit-timing nodig.
    if (gpioSetAlertFuncEx(m_gpioPin, &BiteLockDriver::isrTrampoline, this) < 0) {
        emit errorOccurred(QStringLiteral("kon geen edge-callback registreren op GPIO %1")
                                .arg(m_gpioPin));
        return;
    }

    // Meteen het HUIDIGE niveau emitten (zie de doc-comment bij
    // lockChanged() in de .h) — zonder dit zou de UI pas iets tonen na de
    // EERSTE fysieke flank, terwijl het signaal bij het opstarten van
    // gpsdo_app heel goed al langere tijd constant kan staan (bv. de
    // Rb-bron was al gelockt vóórdat de app opstartte).
    const int initialLevel = gpioRead(m_gpioPin);
    if (initialLevel == PI_BAD_GPIO) {
        emit errorOccurred(QStringLiteral("kon initieel niveau niet lezen op GPIO %1")
                                .arg(m_gpioPin));
        return;
    }
    handleEdge(initialLevel);
}

void BiteLockDriver::stop() {
    if (m_pigpioReady) {
        gpioSetAlertFuncEx(m_gpioPin, nullptr, nullptr);
        PigpioGuard::release();
        m_pigpioReady = false;
    }
    if (m_thread.isRunning()) {
        m_thread.quit();
        m_thread.wait();
    }
}

// Statische C-stijl trampoline, zoals pigpio's gpioSetAlertFuncEx() vereist
// — userData is de BiteLockDriver-instantie (meegegeven in start()).
void BiteLockDriver::isrTrampoline(int gpio, int level, quint32 tick, void *userData) {
    Q_UNUSED(gpio);
    Q_UNUSED(tick); // geen bit-timing nodig voor dit trage, statische signaal
    auto *self = static_cast<BiteLockDriver *>(userData);
    if (self)
        self->handleEdge(level);
}

void BiteLockDriver::handleEdge(int level) {
    // level 0 = LOW = locked, level 1 = HIGH = unlocked/opwarmen (zie
    // BiteLockDriver.h voor de signaalbetekenis) — PI_TIMEOUT (2) kan hier
    // niet voorkomen omdat er bewust geen gpioSetWatchdog() geregistreerd
    // is, maar defensief toch uitsluiten i.p.v. aannemen dat level altijd
    // 0/1 is.
    if (level != 0 && level != 1)
        return;
    emit lockChanged(level == 0);
}
