#pragma once
//
// Mcp3426Adc.h — I2C-driver voor de MCP3426 16-bit ADC (Microchip), gebruikt
// om de Efratom LPRO-101 J1-lampspanning (pin 5) en J1-xtal/EFC-spannings-
// monitor (pin 9) uit te lezen, elk via een externe spanningsdeler (de
// interne referentie is maar 2.048V, de LPRO-signalen lopen tot ~14V).
//
// Registerlayout en I2C-protocol zijn gecontroleerd tegen twee onafhankelijke
// bronnen (Microchip's eigen productspecificaties + de gevestigde open-source
// python-MCP342x-driver van Steve Marple), niet zomaar uit het geheugen:
//
//  Configuratiebyte (1 byte, geschreven vóór elke one-shot-conversie):
//    bit7    RDY   : schrijf 1 om een nieuwe one-shot-conversie te starten;
//                    bij het terug lezen: 0 = klaar, 1 = conversie loopt nog
//    bit6-5  C1,C0 : kanaalkeuze — 00 = kanaal 1, 01 = kanaal 2 (de MCP3426
//                    heeft maar 2 kanalen; 10/11 bestaan niet op dit device,
//                    die zijn alleen geldig op de 4-kanaals MCP3428)
//    bit4    O/C   : 0 = one-shot, 1 = continuous — wij gebruiken bewust
//                    one-shot, zodat we zelf de timing/polling-cyclus in de
//                    hand houden i.p.v. te moeten gokken of een continue
//                    sample al vers genoeg is
//    bit3-2  S1,S0 : resolutie/sample-rate — 00=12-bit/240sps,
//                    01=14-bit/60sps, 10=16-bit/15sps (11=18-bit bestaat
//                    niet op de 3426/27/28-familie, alleen op 3421/3422/3424)
//    bit1-0  G1,G0 : PGA-gain — 00=1x, 01=2x, 10=4x, 11=8x. Volle schaal is
//                    2.048V / gain; wij gebruiken 1x omdat de spanningsdeler
//                    het signaal toch al naar een klein deel van dat bereik
//                    terugbrengt (zie ChannelConfig::dividerRatio).
//
//  Leesprotocol: 3 bytes terug (2 databytes MSB-first + het actuele
//  configuratiebyte) voor 12/14/16-bit modus. De 2 databytes zijn door de
//  chip zelf al sign-extended two's-complement over de volle 16 bit, dus
//  dezelfde int16-assemblage werkt voor alle drie de resoluties — alleen de
//  18-bit-modus (niet beschikbaar op dit device) heeft een extra databyte.
//
//  I2C-adres: vast per onderdeelnummer-suffix — de kale MCP3426 heeft GEEN
//  adrespinnen (in tegenstelling tot de MCP3427 met 1 en de MCP3428 met 2
//  adrespinnen). Basisadres 0x68 hoort bij suffix "A0"; controleer het
//  daadwerkelijk gekochte onderdeelnummer en pas het adres in start() aan
//  als een andere suffix-variant gebruikt wordt.
//
// NIET automatisch gedaan door deze driver, bewust:
//  - Geen spanningsdeler-verhouding "raden": ChannelConfig::dividerRatio moet
//    empirisch met een multimeter tegen de echte LPRO-101 J1-pinnen bepaald
//    worden, net zoals de N/L/H/F-parameters van de VE2ZAZ (zie FllLink.h) —
//    hardware-specifieke kalibratie hoort niet in firmware-defaults.
//
// Bronnen: Microchip MCP3426/7/8-productspecificaties (2.048V ±0.05%
// referentie, PGA 1/2/4/8x, 15/60/240 SPS bij 16/14/12-bit, 2.7-5.5V supply),
// en de configuratiebyte-indeling zoals geïmplementeerd in
// github.com/stevemarple/python-MCP342x (_not_ready_mask/_channel_mask/
// _continuous_mode_mask/_resolution_mask/_gain_mask).

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QString>

class Mcp3426Adc : public QObject {
    Q_OBJECT
public:
    // dividerRatio = (R1+R2)/R2 van de externe spanningsdeler op dit kanaal,
    // zodat: werkelijke_spanning = adc_spanning_op_de_pin * dividerRatio.
    // Zet op 1.0 als er (nog) geen deler gemonteerd is (bv. op de testbank
    // met een kleinere testspanning direct op de ADC-ingang).
    struct ChannelConfig {
        int channel = 1;            // 1 of 2 (MCP3426 heeft alleen deze twee)
        double dividerRatio = 1.0;  // (R1+R2)/R2 — EMPIRISCH bepalen, niet gokken
        QString name;               // "lamp" / "xtal", voor logging en signals
    };

    explicit Mcp3426Adc(QObject *parent = nullptr);
    ~Mcp3426Adc() override;

    // i2cDevice bv. "/dev/i2c-1" (hoofd-I2C-bus op de Pi 3B+ GPIO-header).
    // i2cAddress: 0x68 voor de "A0"-suffix-variant — pas aan indien nodig.
    // pollIntervalMs = tijd tussen twee volledige rondes (beide kanalen).
    // Moet ruim boven de conversietijd liggen: bij 16-bit/15sps is een
    // enkele conversie ~67ms, dus 2 kanalen samen ~135ms minimum. Voor deze
    // langzame DC-signalen (lampspanning/xtal-spanning) is 1000ms een ruime,
    // comfortabele marge en meer dan snel genoeg.
    //
    // Start de eigen achtergrondthread (I2C is een blokkerende systeemcall;
    // dit mag de QML/UI-thread niet raken). Voltage-/foutmeldingen komen via
    // de signalen hieronder, die Qt automatisch veilig naar de ontvangende
    // thread doorzet (queued connection) — geen extra locking nodig aan de
    // kant van de aanroeper.
    void start(const QString &i2cDevice, quint8 i2cAddress,
               const ChannelConfig &ch1, const ChannelConfig &ch2,
               int pollIntervalMs = 1000);
    void stop();

signals:
    // voltage = werkelijke spanning ná toepassen van dividerRatio (Volt).
    // rawCode = onbewerkte 16-bit two's-complement ADC-uitlezing (op de ADC-
    // pin, vóór de dividerRatio-vermenigvuldiging), handig voor debug/kalibratie.
    void voltageRead(const QString &channelName, double voltage, qint16 rawCode);

    // Vuurt bij een open()/ioctl()/read()-fout, of als een conversie na
    // kMaxPollAttempts nog steeds niet klaar is (hardware hangt, verkeerd
    // adres, kabel los, etc.). Dit is precies het soort stale-data-detectie
    // dat in de oude BASCOM-firmware ontbrak (zie projectbrief, "Review oude
    // BASCOM-firmware") — hier dus vanaf het begin ingebouwd.
    void errorOccurred(const QString &message);

private slots:
    void init();      // draait op m_thread net na start()
    void pollOnce();  // draait op m_thread, 1x per pollIntervalMs

private:
    bool writeConfig(quint8 configByte);
    bool readResult(quint8 &configByteOut, qint16 &rawCodeOut);
    bool readChannel(const ChannelConfig &cfg, qint16 &rawCodeOut);

    QThread m_thread;
    QTimer *m_timer = nullptr;

    QString m_i2cDevice;
    quint8  m_i2cAddress = 0x68;
    int     m_pollIntervalMs = 1000;
    int     m_fd = -1;

    ChannelConfig m_ch1;
    ChannelConfig m_ch2;

    // --- Configuratiebyte-bouwstenen (zie uitleg bovenaan dit bestand) -----
    static constexpr quint8 kStartConversion = 0x80; // bit7 RDY=1: start one-shot
    static constexpr quint8 kChannel1Bits     = 0x00; // C1C0 = 00
    static constexpr quint8 kChannel2Bits     = 0x20; // C1C0 = 01
    static constexpr quint8 kOneShotMode      = 0x00; // O/C = 0
    static constexpr quint8 kResolution16Bit  = 0x08; // S1S0 = 10 (16-bit/15sps)
    static constexpr quint8 kGain1x           = 0x00; // G1G0 = 00 (PGA 1x)
    static constexpr quint8 kNotReadyMask     = 0x80; // zelfde als kStartConversion,
                                                        // maar hier als leesmasker

    // 16-bit/15sps => 1 conversie duurt ~66.7ms. Eerst die tijd wachten
    // voordat er überhaupt gepolld wordt, dan nog een paar keer met korte
    // tussenpozen als marge (trage bus, scheduling-jitter e.d.).
    static constexpr int kConversionTimeMs   = 70;
    static constexpr int kPollRetryDelayMs   = 10;
    static constexpr int kMaxPollAttempts    = 20; // 70 + 20*10 = 270ms totale marge

    static constexpr double kVref = 2.048; // Volt, intern vast, bij PGA=1x
};
