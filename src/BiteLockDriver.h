#pragma once
//
// BiteLockDriver.h — driver voor het BITE-lock-detect-signaal van de
// Efratom LPRO-101 (J1-pin 6), besloten 17-08-2026 (zie de projectbrief-
// sectie "Lock-detect — BITE-signaal LPRO-101 op GPIO27"). Veel eenvoudiger
// dan Tsic506Driver: dit is een traag/statisch digitaal signaal (geen
// ~125µs-ZACwire-bit-timing), dus gewoon een pigpio-level-read + edge-
// callback — geen bit-decodering, geen Tstrobe-referentiemeting nodig.
//
// GPIO-toewijzing (besloten 17-08-2026): **BCM-GPIO 27** (fysieke
// header-pin 13), via een resistieve niveauverlaging (R1=3,0kΩ/R2=5,6kΩ,
// zie "Spanningsdelers — dimensionering" in de projectbrief) — het
// BITE-signaal zelf gaat tot 4,2–4,8V (HIGH), ver boven wat de Pi-GPIO's
// verdragen (absolute max ≈ 3,8V), vandaar de deler vóór GPIO27.
//
// Signaalbetekenis (LPRO-101 J1-pin 6, "BITE OUTPUT"): LOW (<50mV, na de
// deler dus ook laag) = LOCKED, HIGH (4,2–4,8V vóór de deler, ~3,1–3,3V
// erná) = opwarmen/UNLOCKED. UI-teksten bevestigd door Wilfred
// (17-08-2026): LOW -> **"Atomic lock"**, HIGH -> **"Warm-up"** — die
// vertaling gebeurt in GpsdoModel::biteLockText(), niet hier; deze driver
// geeft alleen de rauwe boolean terug.
//
// Gebruikt dezelfde pigpio-DMA-gebaseerde edge-callback als Tsic506Driver
// (gpioSetAlertFuncEx(), GEEN gpioSetISRFuncEx() — die laatste gebruikt
// intern het op deze Pi/kernel kapotte sysfs-GPIO-mechanisme, zie
// Tsic506Driver.h/de projectbrief voor de volledige bugfix-geschiedenis),
// puur voor consistentie/hergebruik van dezelfde, al-geverifieerde
// GPIO-aanpak — niet omdat dit signaal de DMA-precisie van pigpio nodig
// heeft (een simpele libgpiod-polling zou voor dit trage signaal ook
// prima werken, zie de projectbrief). Init/terminate van pigpio zelf loopt
// via de gedeelde `PigpioGuard` (ref-geteld, proces-globaal) — zie
// PigpioGuard.h — omdat Tsic506Driver in hetzelfde proces ook pigpio
// gebruikt en gpioInitialise()/gpioTerminate() niet per-driver aan te
// roepen zijn zonder de ander te storen.
//
// **Nog NIET empirisch tegen echte hardware getest** — dezelfde
// caveat als Tsic506Driver oorspronkelijk had: gebouwd op basis van de
// datasheet-/besluit-specificatie hierboven, nog te verifiëren zodra het
// BITE-signaal fysiek op GPIO27 hangt. Gebruik
// `gpsdo_test_cli --bite 27` voor losse verificatie vóór hier volledig op
// vertrouwd wordt in de UI (zelfde volgorde als destijds bij de TSic 506F).

#include <QObject>
#include <QThread>

class BiteLockDriver : public QObject {
    Q_OBJECT
public:
    explicit BiteLockDriver(QObject *parent = nullptr);
    ~BiteLockDriver() override;

    // gpioPin = BCM-GPIO-nummer. Wilfreds besloten pin: **27** (zie
    // doc-comment bovenaan) — geen hardcoded default in deze klasse zelf,
    // zelfde patroon als Tsic506Driver::start(gpioPin).
    void start(int gpioPin);
    void stop();

signals:
    // locked = true bij LOW (locked), false bij HIGH (unlocked/opwarmen).
    // Vuurt zowel bij elke fysieke niveauwissel als (eenmalig, direct na
    // start()) met het HUIDIGE niveau — zodat de UI niet hoeft te wachten
    // op de eerste flank als het signaal al langere tijd constant staat
    // (bv. de Rb-bron was al gelockt vóór het opstarten van gpsdo_app).
    //
    // Let op: dit signaal wordt ge-emit vanuit de pigpio-callbackthread
    // (niet vanuit m_thread) — met Qt's signal/slot-mechanisme altijd
    // veilig zolang de ontvangende kant een draaiende event-loop heeft
    // (auto-connection wordt dan automatisch een queued connection), zelfde
    // precedent als Tsic506Driver::temperatureRead().
    void lockChanged(bool locked);

    // Vuurt bij een pigpio-initialisatiefout. BEWUST GEEN pigpio-watchdog
    // hier (in tegenstelling tot Tsic506Driver's 500ms-watchdog) — pigpio's
    // gpioSetWatchdog() staat sowieso maximaal 60000ms (60s) toe, veel te
    // kort om zinnig onderscheid te maken tussen "signaal is uren stabiel
    // LOCKED" (normaal, gewenst gedrag — de Rb-bron kan dagenlang gewoon
    // gelockt blijven) en een echte storing. Een blijvend constant niveau
    // is hier dus GEEN foutconditie, in tegenstelling tot bij de 10Hz-
    // ZACwire-sensor.
    void errorOccurred(const QString &message);

private:
    static void isrTrampoline(int gpio, int level, quint32 tick, void *userData);
    void handleEdge(int level);

    QThread m_thread;
    int m_gpioPin = -1;
    bool m_pigpioReady = false;
};
