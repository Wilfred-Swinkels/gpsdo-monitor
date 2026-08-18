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
// stijgflank) t.o.v. een Tstrobe-drempel — zie handleEdge()/
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
// **Drempel empirisch bevestigd (18-08-2026)** via het `byteDecoded()`-
// diagnose-signaal (zie hieronder) op echte hardware: bij Tstrobe≈64µs
// clusteren databit-lage-fase-duren heel strak op ~32µs ("1") en ~96µs
// ("0") — dat is **0,5×Tstrobe en 1,5×Tstrobe**, NIET 1× en 2× zoals de
// oorspronkelijke aanname. De oude drempel (1,5×Tstrobe = 96) lag daarmee
// bovenop de "0"-cluster i.p.v. er veilig tussenin — een paar µs normale
// meetjitter (95 i.p.v. 96, ruimschoots binnen de waargenomen spreiding)
// duwde een "0"-bit dan zomaar over de drempel heen naar "1", wat precies
// de intermitterende pariteits-/framing-fouten verklaart. **Gefixt:**
// `kBitThresholdFactor` van 1,5 naar **1,0×Tstrobe** — exact het midden
// tussen de twee clusters (32 en 96), dus ~30µs marge aan weerszijden
// i.p.v. bijna 0. Consistent met de BASCOM-referentie: die wacht exact
// Tstrobe µs en bemonstert dan — bij een "1"-bit (~32µs) is de lijn op dat
// moment allang weer hoog, bij een "0"-bit (~96µs) nog steeds laag; dat IS
// letterlijk een 1×Tstrobe-drempel.
//
// Nog OPEN, nog niet empirisch geverifieerd:
//  - De polariteit van het pariteitsbit (even parity: aangenomen dat het
//    totaal aantal 1-bits inclusief pariteitsbit even moet zijn) — de
//    drempelfix hierboven loste de meeste pariteitsfouten al op, dus dit
//    lijkt inmiddels ook impliciet bevestigd, maar nog niet expliciet
//    tegen een edge case getest.
//  - Of er een externe pull-up weerstand nodig is op de signaalpin — bleek
//    bij een eerdere test geen merkbaar effect te hebben; blijkt nu
//    waarschijnlijk niet nodig te zijn geweest, de drempel was de échte
//    oorzaak. Kan alsnog blijven zitten (kan geen kwaad).
//
// Vereist libpigpio (niet pigpiod-daemon — deze driver linkt rechtstreeks
// tegen de library en praat zelf met /dev/gpiomem, dus gpioInitialise()
// draait in-process). Dat vereist root-rechten, wat toch al zo is omdat
// gpsdo_app als systemd-service met User=root draait (zie
// systemd/gpsdo-monitor.service) — geen extra permissie-gedoe dus. Zorg
// dat de pigpiod-daemon (als die ooit los geïnstalleerd wordt) NIET
// tegelijk draait: die claimt dezelfde GPIO-hardware exclusief.
//
// pigpio-init/-terminate loopt sinds 18-08-2026 via de gedeelde
// `PigpioGuard` (ref-geteld) i.p.v. hier rechtstreeks gpioInitialise()/
// gpioTerminate() aan te roepen — nodig sinds `BiteLockDriver` er als
// tweede pigpio-gebruiker in hetzelfde proces bij kwam, zie PigpioGuard.h.
//
// Bronnen: IST AG ZACwire-interface-appnote (protocol-framing, bit-
// encodering, timing), IST AG TSic-506-datasheet (omzetformule, bereik,
// nauwkeurigheid, pinout, voeding), en de open-source referentie-
// implementatie github.com/grillbaer/python-tsic (Raspberry Pi + pigpio,
// zelfde sensorfamilie) plus aanvullende onafhankelijke implementaties ter
// controle van de bit-encoderingsregel.

#include <QObject>
#include <QThread>
#include <QVector>

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
    // realtime OS). Zet ook pigpio's DMA-sample-interval op 1µs i.p.v. de
    // standaard 5µs (zie .cpp) — een poging om meetruis op de korte
    // (~tientallen µs) lage-fase-duren te verkleinen; nog niet empirisch
    // bevestigd of dit daadwerkelijk helpt.
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

    // Diagnose-signaal (18-08-2026), toegevoegd om de resterende foutfre-
    // quentie te kunnen onderzoeken zonder een logic-analyzer: vuurt voor
    // ÉLKE poging om een byte te decoderen (geslaagd én mislukt), met de
    // gemeten Tstrobe-referentie (µs) en de rauwe lage-fase-duur (µs) van
    // elk van de 9 bits in dat byte (8 data + 1 pariteit, in ontvangst-
    // volgorde). Zo is direct te zien of de "0"-cluster echt rond 2×Tstrobe
    // zit, hoeveel jitter erop zit, en of de 1,5×Tstrobe-drempel
    // (kBitThresholdFactor) goed gekozen is — i.p.v. blind aan de
    // parameters te blijven draaien.
    void byteDecoded(quint32 strobeUs, QVector<quint32> bitDurationsUs,
                      quint8 byteValue, bool parityOk);

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
    QVector<quint32> m_bitDurations; // diagnose: rauwe lage-fase-duur (µs) per bit, zie byteDecoded()
    quint8 m_byteValue = 0;
    quint8 m_parityAccum = 0;    // lopende XOR van de 8 databits

    // --- Pakket-niveau state (2 bytes per complete meting) --------------
    int m_packetIndex = 0;       // 0 = wacht op hoge-bits-pakket, 1 = wacht op lage-bits-pakket
    quint8 m_highByte = 0;

    // Beslisgrens t.o.v. Tstrobe om een lage-fase als "kort"(=1)/"lang"(=0)
    // te classificeren. Empirisch bevestigd (18-08-2026, zie doc-comment
    // boven): de twee clusters zitten op 0,5×Tstrobe en 1,5×Tstrobe, dus
    // 1,0×Tstrobe is exact het midden — maximale marge (~30µs) aan
    // weerszijden. (Was eerst 1.5, wat bovenop de "0"-cluster lag i.p.v.
    // ertussenin — zie de bugfix-toelichting boven.)
    static constexpr double kBitThresholdFactor = 1.0;

    // pigpio-watchdog: als er dit lang geen enkele edge is op de pin, roept
    // pigpio de callback met level=2 aan ("timeout") — gebruiken we om
    // vastgelopen/halverwege-onderbroken frames te resetten én (na de
    // eerste keer) een errorOccurred te melden dat er niets binnenkomt.
    static constexpr unsigned kWatchdogTimeoutMs = 500; // ruim > 100ms (10Hz sample-interval)
};
