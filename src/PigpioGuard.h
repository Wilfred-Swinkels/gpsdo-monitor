#pragma once
//
// PigpioGuard.h — proces-brede, ref-geteld beheer van pigpio's
// gpioInitialise()/gpioTerminate(), toegevoegd (18-08-2026) toen er een
// TWEEDE pigpio-gebruiker (BiteLockDriver, naast het al bestaande
// Tsic506Driver) in hetzelfde proces (gpsdo_app) bij kwam.
//
// WAAROM dit nodig is: gpioInitialise()/gpioTerminate() zijn PROCES-
// globaal, niet per-instantie/per-driver — als élke driver z'n eigen
// ongecoördineerde gpioInitialise()-in-start()/gpioTerminate()-in-stop()
// -paar blijft aanroepen (zoals Tsic506Driver dat tot nu toe deed), dan
// sluit de driver die als EERSTE stopt (bv. tijdens een test/herstart) met
// z'n eigen gpioTerminate() de DMA-/GPIO-subsystemen af voor ALLE andere
// pigpio-gebruikers in hetzelfde proces, ook als die nog gewoon actief
// zijn — dat zou bv. de TSic 506F-temperatuurmeting stilletjes laten
// stoppen zodra de BITE-driver om wat voor reden dan ook stopt. Deze
// kleine klasse telt actieve gebruikers en initialiseert/termineert pigpio
// maar precies één keer, ongeacht hoeveel drivers 'm gebruiken of in welke
// volgorde ze starten/stoppen.
//
// Bijkomend voordeel: `gpioCfgClock()` (het 1µs-DMA-sample-interval, zie
// Tsic506Driver) moet vóór de EERSTE gpioInitialise()-aanroep in het
// proces gebeuren — na die eerste aanroep heeft een latere gpioCfgClock()
// stilzwijgend geen effect meer. Met de oude, ongecoördineerde aanpak zou
// de volgorde waarin drivers starten dit stilletjes kunnen laten mislukken
// (als BiteLockDriver toevallig als eerste gpioInitialise() aanriep, vóór
// Tsic506Driver z'n gpioCfgClock(1,1,0) kon zetten). Nu gebeurt die
// gpioCfgClock()-aanroep hier, éénmalig, vóór de daadwerkelijke
// gpioInitialise() — ongeacht welke driver toevallig als eerste acquire()
// aanroept.
//
// Thread-safety: acquire()/release() zijn bewust met een mutex beschermd,
// ook al roepen alle huidige drivers dit vooralsnog vanuit hun eigen
// QThread::start()-pad aan (dus in de praktijk sequentieel vanuit
// main()) — een mutex hier is goedkoop en voorkomt een subtiele
// race-conditie mocht dat ooit veranderen.

#include <QMutex>

class PigpioGuard {
public:
    // Verhoogt de gebruikersteller. Retourneert true zodra pigpio klaar is
    // voor gebruik (was al geïnitialiseerd door een andere gebruiker, of is
    // nu net gelukt); false als de allereerste gpioInitialise() zelf
    // mislukte (bv. geen root-rechten) — in dat geval blijft de teller op 0
    // staan, dus de aanroeper hoeft release() niet aan te roepen na een
    // false-return.
    static bool acquire();

    // Verlaagt de gebruikersteller; roept gpioTerminate() aan zodra de
    // teller op 0 komt. Alleen aanroepen na een geslaagde (true) acquire().
    static void release();

private:
    static QMutex s_mutex;
    static int s_refCount;
};
