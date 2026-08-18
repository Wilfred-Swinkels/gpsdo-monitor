#pragma once
//
// Tsic506Driver.h — driver voor de IST AG TSic 506F digitale temperatuur-
// sensor (TO92, pin1=GND pin2=Signaal pin3=VDD), uitgelezen via het
// eigen ZACwire(R)-eendraads-protocol van de sensor op een enkele Pi-GPIO-
// pin. Toegevoegd naast (niet i.p.v.) de MCP3426-ADC — die blijft gewoon
// gepland voor de lamp-/xtal-spanning van de LPRO-101 zodra de PCB er is.
//
// Aanleiding/doel (Wilfred, 16-08-2026): temperatuur meten in de buurt van
// de Rb-fysicapakket, om eventuele Δf/f-drift te kunnen correleren met
// omgevingstemperatuur.
//
// GPIO-toewijzing (Wilfred, 16-08-2026): **BCM-GPIO 17** voor het ZACwire-
// signaal van de TSic 506F. De MCP3426-ADC gaat op de standaard I2C1-bus
// (BCM-GPIO 2 = SDA, BCM-GPIO 3 = SCL, `/dev/i2c-1`) — dat is de normale,
// vaste I2C-pinnenpaar op de Pi-header en vereist geen aparte toewijzing in
// deze code. Exacte plaatsing van de sensor t.o.v. de Rb-bron en of er een
// pull-up nodig is op de signaaldraad zijn nog NIET bevestigd/empirisch
// bepaald — zie de driver-brede caveat verderop in dit bestand.
//
// ZACwire is GEEN I2C/SPI maar een eigen, zelfklokkend eendraads-protocol
// (IST AG). Layout hieronder gecontroleerd tegen de officiële IST AG
// ZACwire-appnote plus meerdere onafhankelijke open-source-implementaties
// (met name grillbaer/python-tsic, een Raspberry Pi + pigpio-referentie-
// implementatie voor precies deze sensorfamilie):
//
//  Eén complete meting = 2 pakketten, elk: startbit + 8 databits +
//  1 (even-)pariteitsbit, MSB eerst. Tussen pakketten (en na het tweede,
//  tot de volgende meting) staat de lijn 1 bit-venster lang hoog ("stop").
//  Voor 11-bit-sensors zoals de TSic 506:
//    pakket 1 = de 3 hoogste bits van de 11-bit ruwe waarde (gepad met
//               voorloopnullen tot een volledig byte)
//    pakket 2 = de 8 laagste bits
//    ruw (0..2047) = pakket1_byte * 256 + pakket2_byte
//
//  Bit-encodering (zelfklokkend, geen vaste baudrate-aanname nodig): de
//  LAGE fase van de STARTbit is de referentie ("Tstrobe") voor de rest van
//  dát byte — elk volgend databit/pariteitsbit heeft ook een lage fase
//  gevolgd door een hoge fase, en de DUUR van die lage fase t.o.v. Tstrobe
//  bepaalt de bitwaarde: korter dan Tstrobe = 1, langer dan Tstrobe = 0.
//  Nominale bitrate ~8kHz (~125µs/bit-venster) — vandaar dat dit NIET met
//  gewone sysfs/libgpiod-polling betrouwbaar te decoderen is; dit gebruikt
//  pigpio (DMA-based edge-timestamping, µs-resolutie) i.p.v. een QTimer-
//  polling-lus zoals Mcp3426Adc.
//
//  Temperatuuromzetting, SPECIFIEK voor de TSic 506F (11-bit, bereik
//  -10°C..+60°C — bevestigd een ANDERE formule dan andere TSic-varianten
//  zoals de 306/716 met hun eigen bereik/resolutie):
//    T[°C] = (ruw / 2047.0) * 70.0 - 10.0
//
//  Overige datasheet-specs (IST AG): nauwkeurigheid ±0.1K in +5..+45°C,
//  ±0.2K over het volle bereik; resolutie 0.034K; sample rate 10Hz;
//  voeding 3.0-5.5V, typ. 30µA @ 3.3V/25°C.
//
// Bit-decodering: duur-meting van de volledige lage fase (valflank tót
// stijgflank) t.o.v. een 1,5×Tstrobe-drempel — zie handleEdge()/
// handleLowPulse() in de .cpp. Op 18-08-2026 kort geprobeerd te vervangen
// door een "wacht na de valflank Tstrobe µs, bemonster dan het pinniveau"-
// aanpak (naar analogie van een werkende BASCOM/ATmega-referentie die
// Wilfred aandroeg) — dat bleek NIET compatibel met hoe pigpio's
// `gpioSetAlertFuncEx()` callbacks aflevert (in batches, met tot ~1ms
// latency t.o.v. de fysieke flank), en gaf op echte hardware juist MEER
// corrupte frames. Teruggedraaid; zie de uitgebreide toelichting in de
// .cpp bij handleEdge(). De duur-meting hieronder gebruikt alleen de
// pigpio-`tick`-waarden (accurate DMA-sample-timestamps, ongevoelig voor
// afleverlatency), en is daarom wél compatibel met `gpioSetAlertFuncEx()`.
//
// Nog OPEN, nog niet empirisch geverifieerd:
//  - De exacte drempel waarmee een lage-fase-duur als "1" of "0" bestempeld
//    wordt (`kBitThresholdFactor`, hieronder 1,5×Tstrobe) is nog NIET tegen
//    een echte TSic 506F met een logic-analyzer/scope geverifieerd — alleen
//    tegen de spec + andere implementaties op papier. Wel al deels indirect
//    bevestigd: eerste hardwaretests gaven af en toe een volledig correcte,
//    plausibele meting (24,4°C), dus de drempel/polariteit zitten in de
//    juiste orde van grootte — de resterende foutfrequentie lijkt eerder
//    ruis/signaalintegriteit dan een verkeerde drempel.
//  - De polariteit van het pariteitsbit (even parity: aangenomen dat het
//    totaal aantal 1-bits inclusief pariteitsbit even moet zijn).
//  - Of er een externe pull-up weerstand nodig is op de signaalpin (sommige
//    TSic-varianten/schakelingen willen dat, andere niet) — controleren in
//    het datasheet/de gekochte module vóór het aansluiten. Meest waarschijn-
//    lijke verklaring voor de resterende foutfrequentie na de pigpio-
//    ISR→Alert-bugfix (zie projectbrief).
//  - Voedingsspanning van de sensor (3.0-5.5V spec) t.o.v. GPIO17's
//    3,3V-tolerantie — controleren dat de sensor NIET op de 5V-rail hangt.
//    Nog geen antwoord van Wilfred op deze vraag.
//
// Vereist libpigpio (niet pigpiod-daemon — deze driver linkt rechtstreeks
// tegen de library en praat zelf met /dev/gpiomem, dus gpioInitialise()
// draait in-process). Dat vereist root-rechten, wat toch al zo is omdat
// gpsdo_app als systemd-service met User=root draait (zie
// systemd/gpsdo-monitor.service) — geen extra permissie-gedoe dus. Zorg
// dat de pigpiod-daemon (als die ooit los geïnstalleerd wordt) NIET
// tegelijk draait: die claimt dezelfde GPIO-hardware exclusief.
//
// Bronnen: IST AG ZACwire-interface-appnote (protocol-framing, bit-
// encodering, timing), IST AG TSic-506-datasheet (omzetformule, bereik,
// nauwkeurigheid, pinout, voeding), en de open-source referentie-
// implementatie github.com/grillbaer/python-tsic (Raspberry Pi + pigpio,
// zelfde sensorfamilie) plus aanvullende onafhankelijke implementaties ter
// controle van de bit-encoderingsregel.

#include <QObject>
#include <QThread>

class Tsic506Driver : public QObject {
    Q_OBJECT
public:
    explicit Tsic506Driver(QObject *parent = nullptr);
    ~Tsic506Driver() override;

    // gpioPin = BCM-GPIO-nummer (niet de fysieke pinheader-positie) waarop
    // de TSic 506F's Signaal-pin (pin 2) is aangesloten. Wilfreds gekozen
    // pin: **17** (zie doc-comment bovenaan dit bestand) — geen hardcoded
    // default in deze klasse zelf, blijft een expliciete parameter net als
    // Mcp3426Adc::start()'s device/adres-argumenten; zie
    // gpsdo_test_cli --tsic <gpio> (default 17) voor losse hardware-
    // verificatie vóór dit in de UI hangt.
    //
    // Start de eigen achtergrondthread + registreert een pigpio edge-
    // callback op gpioPin via gpioSetAlertFuncEx() (DMA-based sampling,
    // µs-timestamps, GEEN sysfs-GPIO-interrupt — zie de bugfix-toelichting
    // in de .cpp voor waarom bewust niet gpioSetISRFuncEx() gebruikt wordt.
    // Dit is GEEN polling-lus zoals bij Mcp3426Adc, want ZACwire's ~125µs
    // bit-venster is te snel voor een QTimer-polling-aanpak op een niet-
    // realtime OS).
    void start(int gpioPin);
    void stop();

signals:
    // celsius = omgerekende temperatuur via de TSic-506F-specifieke
    // formule (zie bovenaan dit bestand). rawValue = ruwe 11-bit waarde
    // (0..2047) vóór omrekening, handig voor debug.
    //
    // Let op: dit signaal wordt ge-emit vanuit de pigpio-callbackthread
    // (niet vanuit m_thread) — dat is met Qt's signal/slot-mechanisme
    // altijd veilig zolang de ontvangende kant een draaiende event-loop
    // heeft (auto-connection wordt dan automatisch een queued connection).
    void temperatureRead(double celsius, quint16 rawValue);

    // Vuurt bij: een pariteitsfout in een ontvangen byte, een ongeldige
    // (>2047) ruwe waarde, een pigpio-initialisatiefout, of - via pigpio's
    // eigen watchdog-timeout - langdurige stilte op de pin (sensor niet
    // aangesloten/geen voeding/verkeerde GPIO).
    void errorOccurred(const QString &message);

private:
    static void isrTrampoline(int gpio, int level, quint32 tick, void *userData);
    void handleEdge(int level, quint32 tick);
    void handleLowPulse(quint32 lowDurationUs);
    void onByteComplete(quint8 byteValue, bool parityOk);
    void resetFrameState();

    QThread m_thread;
    int m_gpioPin = -1;
    bool m_pigpioReady = false;

    // --- Bit-niveau state (alleen gewijzigd vanuit de pigpio-callback-
    //     thread; zie handleEdge/handleLowPulse — bewust GEEN mutex nodig
    //     omdat pigpio callbacks voor eenzelfde GPIO niet overlappend
    //     worden aangeroepen). --------------------------------------------
    enum class BitState { Idle, ReceivingByte };
    BitState m_bitState = BitState::Idle;
    quint32 m_lowPhaseStartTick = 0;
    quint32 m_strobeUs = 0;
    int m_bitsReceived = 0;      // 0..8: aantal data-/pariteitsbits na de startbit
    quint8 m_byteValue = 0;
    quint8 m_parityAccum = 0;    // lopende XOR van de 8 databits

    // --- Pakket-niveau state (2 bytes per complete meting) --------------
    int m_packetIndex = 0;       // 0 = wacht op hoge-bits-pakket, 1 = wacht op lage-bits-pakket
    quint8 m_highByte = 0;

    // Marge t.o.v. Tstrobe om een lage-fase als "kort"(=1)/"lang"(=0) te
    // classificeren: gebruik 1.5x Tstrobe als beslisgrens i.p.v. Tstrobe
    // zelf, voor wat ruis-marge tussen de twee verwachte clusters (~1x en
    // ~2x Tstrobe). Zie doc-comment boven voor de status van deze aanname.
    static constexpr double kBitThresholdFactor = 1.5;

    // pigpio-watchdog: als er dit lang geen enkele edge is op de pin, roept
    // pigpio de callback met level=2 aan ("timeout") — gebruiken we om
    // vastgelopen/halverwege-onderbroken frames te resetten én (na de
    // eerste keer) een errorOccurred te melden dat er niets binnenkomt.
    static constexpr unsigned kWatchdogTimeoutMs = 500; // ruim > 100ms (10Hz sample-interval)
};
