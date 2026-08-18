// PigpioGuard.cpp
//
// Zie PigpioGuard.h voor het waarom. Simpele ref-telling met een mutex.

#include "PigpioGuard.h"

#include <pigpio.h>

QMutex PigpioGuard::s_mutex;
int PigpioGuard::s_refCount = 0;

bool PigpioGuard::acquire() {
    QMutexLocker locker(&s_mutex);
    if (s_refCount > 0) {
        ++s_refCount;
        return true;
    }

    // Alleen bij de ALLEREERSTE acquire() in het proces: het DMA-
    // sample-interval verkleinen van pigpio's standaard 5µs naar 1µs (zie
    // Tsic506Driver.h voor de motivatie — nog niet empirisch bevestigd of
    // dit daadwerkelijk helpt, maar kan geen kwaad). Moet vóór
    // gpioInitialise() gebeuren, en heeft daarna geen effect meer.
    // cfgPeripheral=1 (PCM) i.p.v. 0 (PWM) om geen conflict te riskeren
    // mocht er ooit iets anders op deze Pi de hardware-PWM gebruiken.
    gpioCfgClock(1, 1, 0);

    if (gpioInitialise() < 0)
        return false;

    s_refCount = 1;
    return true;
}

void PigpioGuard::release() {
    QMutexLocker locker(&s_mutex);
    if (s_refCount <= 0)
        return; // release() zonder geslaagde acquire() ervoor — negeren i.p.v. crashen
    --s_refCount;
    if (s_refCount == 0)
        gpioTerminate();
}
