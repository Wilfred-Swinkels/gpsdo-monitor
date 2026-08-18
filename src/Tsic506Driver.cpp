// Tsic506Driver.cpp
//
// Zie Tsic506Driver.h voor de volledige protocol-uitleg en bronvermelding.
//
// Net als Mcp3426Adc heeft deze klasse een eigen QThread, maar om een
// andere reden: hier is het niet om blokkerende I/O van de QML/UI-thread
// weg te houden, maar omdat pigpio's edge-interrupt-callback sowieso op
// zijn EIGEN (niet-Qt) achtergrondthread draait, onafhankelijk van welke
// thread start()/stop() aanroept. m_thread bestaat vooral zodat deze
// QObject niet op de aanroepende (UI-)thread hoeft te leven, consistent
// met de andere driver-klassen in deze codebase.

#include "Tsic506Driver.h"
#include "PigpioGuard.h"

#include <QDebug>

#include <pigpio.h>

Tsic506Driver::Tsic506Driver(QObject *parent) : QObject(parent) {
    moveToThread(&m_thread);
    m_thread.start();
}

Tsic506Driver::~Tsic506Driver() {
    stop();
}

void Tsic506Driver::start(int gpioPin) {
    m_gpioPin = gpioPin;
    resetFrameState();

    // gpioInitialise()/gpioCfgClock() zijn PROCES-globaal, niet per-
    // instantie — sinds BiteLockDriver (18-08-2026) er als tweede pigpio-
    // gebruiker in hetzelfde proces bij kwam, lopen die twee via de
    // gedeelde, ref-getelde PigpioGuard i.p.v. hier rechtstreeks
    // gpioInitialise()/gpioTerminate() aan te roepen — zie PigpioGuard.h
    // voor het waarom (een los gpioTerminate() vanuit de ene driver zou
    // anders ook de andere driver stilletjes afsluiten). Het 1µs-DMA-
    // sample-interval-experiment (was hier eerst een losse gpioCfgClock()-
    // aanroep, zie de projectbrief voor de motivatie — nog NIET empirisch
    // bevestigd of dit daadwerkelijk helpt) zit nu in PigpioGuard::acquire(),
    // vóór de eerste gpioInitialise() in het proces.
    if (!PigpioGuard::acquire()) {
        emit errorOccurred(QStringLiteral(
            "pigpio-initialisatie faalde — draait dit proces als root, en is "
            "de pigpiod-daemon (indien geïnstalleerd) uitgeschakeld?"));
        return;
    }
    m_pigpioReady = true;

    gpioSetMode(m_gpioPin, PI_INPUT);

    // Bugfix (18-08-2026), gevonden bij Wilfreds eerste hardwaretest: dit
    // registreerde oorspronkelijk gpioSetISRFuncEx(). Die functie gebruikt
    // intern het Linux sysfs-GPIO-mechanisme (schrijft naar
    // /sys/class/gpio/export om een echte kernel-interrupt te krijgen —
    // zie pigpio's eigen documentatie: "The underlying Linux sysfs GPIO
    // interface is used to provide the interrupt services"). Op Raspberry
    // Pi OS Bookworm (kernel 6.6.x — exact wat op deze Pi 3B+ draait) is
    // die sysfs-export kapot: schrijven naar /sys/class/gpio/export geeft
    // "Invalid argument" (dmesg: "export_store: invalid GPIO N"), een
    // bekend, door Raspberry Pi (nog) niet opgelost kernelprobleem dat
    // meerdere GPIO-libraries raakt (pigpio, wiringPi). Vandaar de "kon
    // geen interrupt-handler registreren"-foutmelding.
    // gpioSetAlertFuncEx() is het juiste alternatief: pigpio's eigen
    // DMA-gebaseerde GPIO-sampling, gebruikt geen sysfs — dit was ook
    // altijd al het ontwerp-idee (zie de doc-comments hierboven/in de .h:
    // "DMA-based edge-timestamping"), alleen implementeerde de code per
    // ongeluk de sysfs-ISR-route i.p.v. de DMA-route. gpioSetAlertFuncEx()
    // rapporteert sowieso beide flanken (geen EITHER_EDGE-parameter nodig)
    // en heeft dezelfde callback-signatuur (gpio, level, tick, userdata),
    // dus isrTrampoline()/handleEdge() hoeven niet aangepast te worden. De
    // stilte-watchdog (level=PI_TIMEOUT, zie handleEdge()) zat bij
    // gpioSetISRFuncEx() ingebakken in dezelfde aanroep, maar is bij
    // gpioSetAlertFuncEx() een apart mechanisme — vandaar de losse
    // gpioSetWatchdog()-aanroep hieronder.
    if (gpioSetAlertFuncEx(m_gpioPin, &Tsic506Driver::isrTrampoline, this) < 0) {
        emit errorOccurred(QStringLiteral("kon geen edge-callback registreren op GPIO %1")
                                .arg(m_gpioPin));
        return;
    }
    gpioSetWatchdog(m_gpioPin, kWatchdogTimeoutMs);
}

void Tsic506Driver::stop() {
    if (m_pigpioReady) {
        gpioSetWatchdog(m_gpioPin, 0);
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
// (zelfde signatuur als gpioSetISRFuncEx() had, dus deze functie zelf hoefde
// niet te wijzigen bij de bugfix hierboven) — userData is de
// Tsic506Driver-instantie (meegegeven in start()).
void Tsic506Driver::isrTrampoline(int gpio, int level, quint32 tick, void *userData) {
    Q_UNUSED(gpio);
    auto *self = static_cast<Tsic506Driver *>(userData);
    if (self)
        self->handleEdge(level, tick);
}

// Draait op pigpio's eigen callbackthread, NIET op m_thread. pigpio
// garandeert dat opeenvolgende callbacks voor dezelfde GPIO niet
// overlappend/gelijktijdig binnenkomen, dus de bit-/pakket-state hieronder
// heeft bewust geen mutex nodig (single-threaded toegang, alleen niet
// vanaf de thread die je zou verwachten).
//
// Leerpunt (18-08-2026), teruggedraaid na een mislukt experiment: op basis
// van een werkende BASCOM/ATmega-referentie-implementatie (Wilfred) is hier
// kort een "wacht na de valflank exact Tstrobe µs via gpioDelay(), bemonster
// dan het huidige pinniveau via gpioRead()"-aanpak geprobeerd, i.p.v. de
// duur-meting hieronder. Dat werkte op de ATmega omdat `Config Int0 =
// Falling` daar een ECHTE, synchrone hardware-interrupt is — de ISR vuurt
// exact op het moment van de flank, dus "Tstrobe wachten na binnenkomst"
// betekent ook echt "Tstrobe na de fysieke flank". `gpioSetAlertFuncEx()`
// garandeert dat NIET: pigpio's eigen documentatie zegt expliciet dat de
// callbackthread maar "nominally 1000 times per second" getriggerd wordt en
// callbacks in batches afgeleverd worden — dus tegen de tijd dat onze
// callback met level==0 binnenkomt, kan de ECHTE valflank allang (tot ~1ms)
// in het verleden liggen. `gpioDelay()` wacht vanaf DAT moment (nu), niet
// vanaf de fysieke flank — dus de daaropvolgende `gpioRead()` bemonsterde in
// de praktijk een willekeurig, niet-gesynchroniseerd moment. Resultaat:
// vrijwel alle frames corrupt (erger dan de oorspronkelijke fout). Direct
// teruggedraaid naar de aanpak hieronder.
// De duur-meting (valflank-tick tót stijgflank-tick, vergeleken met
// 1,5×Tstrobe) heeft dit probleem NIET: pigpio's `tick`-waarde is de
// werkelijke DMA-sample-timestamp op het moment van de flank zelf, dus een
// verschil tussen twee ticks blijft correct ongeacht hoe laat de callback
// die ticks aan ons aflevert. Dát maakt duur-meting de juiste aanpak in
// combinatie met `gpioSetAlertFuncEx()` — de resterende foutfrequentie (zie
// projectbrief) is dus waarschijnlijk ruis/pull-up-gerelateerd, niet een
// architectuurfout in de meetmethode zelf.
void Tsic506Driver::handleEdge(int level, quint32 tick) {
    if (level == PI_TIMEOUT) {
        // pigpio-watchdog: kWatchdogTimeoutMs geen edge gezien. Frame
        // resetten (voorkomt dat een halverwege afgebroken transmissie de
        // volgende laat mislukken) en de gebruiker laten weten dat er
        // niets binnenkomt.
        const bool wasIdle = (m_bitState == BitState::Idle && m_packetIndex == 0);
        resetFrameState();
        if (!wasIdle) {
            emit errorOccurred(QStringLiteral(
                "geen ZACwire-signaal meer op GPIO %1 (>%2ms stil) — bekabeling/voeding "
                "van de TSic 506F controleren").arg(m_gpioPin).arg(kWatchdogTimeoutMs));
        }
        return;
    }

    if (level == 0) {
        // Valflank: lage fase van dit bit-venster begint nu.
        m_lowPhaseStartTick = tick;
        return;
    }

    // level == 1 (stijgflank): lage fase is net geëindigd. pigpio's tick is
    // een 32-bit microseconden-teller die na ~72 minuten wrapt; unsigned
    // aftrekken geeft ook over die wrap heen het juiste (kleine) verschil.
    const quint32 lowDurationUs = tick - m_lowPhaseStartTick;
    handleLowPulse(lowDurationUs);
}

void Tsic506Driver::handleLowPulse(quint32 lowDurationUs) {
    if (m_bitState == BitState::Idle) {
        // Dit is de lage fase van de STARTbit van een nieuw byte — die
        // duur is per definitie de Tstrobe-referentie voor de rest van dit
        // byte (zelfklokkend, zie doc-comment in de .h).
        m_strobeUs = lowDurationUs;
        m_bitState = BitState::ReceivingByte;
        m_bitsReceived = 0;
        m_byteValue = 0;
        m_parityAccum = 0;
        m_bitDurations.clear();
        return;
    }

    // Data- of pariteitsbit: korter dan (marge x) Tstrobe = 1, langer = 0.
    const bool bitIsOne = lowDurationUs < static_cast<quint32>(m_strobeUs * kBitThresholdFactor);
    m_bitDurations.append(lowDurationUs); // diagnose, zie byteDecoded()

    if (m_bitsReceived < 8) {
        m_byteValue = static_cast<quint8>((m_byteValue << 1) | (bitIsOne ? 1 : 0));
        m_parityAccum ^= (bitIsOne ? 1 : 0);
        ++m_bitsReceived;
        return;
    }

    // 9e bit = pariteitsbit. Even parity: het pariteitsbit hoort gelijk te
    // zijn aan de XOR van de 8 databits (zodat het totaal aantal 1-bits
    // incl. pariteitsbit even is). Polariteit nog niet hardware-gevalideerd,
    // zie doc-comment in de .h.
    const bool parityBit = bitIsOne;
    const bool parityOk = (parityBit == (m_parityAccum != 0));

    const quint8 completedByte = m_byteValue;
    emit byteDecoded(m_strobeUs, m_bitDurations, completedByte, parityOk);
    m_bitState = BitState::Idle;
    onByteComplete(completedByte, parityOk);
}

void Tsic506Driver::onByteComplete(quint8 byteValue, bool parityOk) {
    if (!parityOk) {
        emit errorOccurred(QStringLiteral("ZACwire-pariteitsfout op GPIO %1 (pakket %2)")
                                .arg(m_gpioPin)
                                .arg(m_packetIndex == 0 ? "1 (hoge bits)" : "2 (lage bits)"));
        m_packetIndex = 0; // frame weggooien, opnieuw beginnen bij het volgende pakket
        return;
    }

    if (m_packetIndex == 0) {
        m_highByte = byteValue;
        m_packetIndex = 1;
        return;
    }

    // Tweede pakket compleet: ruw = hoge_byte*256 + lage_byte (0..2047 voor
    // een 11-bit sensor als de TSic 506).
    m_packetIndex = 0;
    const quint16 raw = (static_cast<quint16>(m_highByte) << 8) | byteValue;
    if (raw > 2047) {
        emit errorOccurred(QStringLiteral(
            "ZACwire: ruwe waarde %1 buiten het geldige 11-bit bereik (0..2047) op GPIO %2 — "
            "waarschijnlijk een framing-/pariteitsfout die niet gedetecteerd is")
                                .arg(raw)
                                .arg(m_gpioPin));
        return;
    }

    // TSic-506F-specifieke omzetting, zie doc-comment in de .h.
    const double celsius = (raw / 2047.0) * 70.0 - 10.0;
    emit temperatureRead(celsius, raw);
}

void Tsic506Driver::resetFrameState() {
    m_bitState = BitState::Idle;
    m_bitsReceived = 0;
    m_byteValue = 0;
    m_parityAccum = 0;
    m_packetIndex = 0;
    m_highByte = 0;
    m_bitDurations.clear();
}
