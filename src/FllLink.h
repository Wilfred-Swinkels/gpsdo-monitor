#pragma once
//
// FllLink.h — seriële link + snel-lock-strategie voor de VE2ZAZ GPS Std FLL-controller.
//
// Gebaseerd op VE2ZAZ's "GPS-Derived 10MHz Frequency Standard — USER MANUAL v1.1"
// (GPS_Std_UserMan_v1.1.pdf), secties 4.3, 4.4.2, 4.4.3, 4.4.4, 4.4.5, 5 en 6.
//
// Strategie in het kort (bronnen: zie .cpp):
//  - Bij opstarten: forceer M02 (Sample Summing) + kleine S (0x000A = 10 samples)
//    zodat de eerste lock zo snel mogelijk komt.
//  - Zodra de FLL een aantal opeenvolgende cycli achter elkaar "L" (Locked)
//    rapporteert: schakel over naar M01 (Sample Voting) + grote S (0x0200 = 512
//    samples), de instelling die de handleiding zelf aanraadt voor de beste
//    langetermijnnauwkeurigheid.
//  - N/L/H/F (negate/lock-limit/holdover-limit/coarse-fine-threshold) worden
//    NIET aangeraakt: de handleiding zegt expliciet dat dit per GPS/VCXO-
//    combinatie empirisch afgesteld moet worden (sectie 4.4.4) — dat is iets
//    om straks op de bank te doen met de echte hardware, niet iets om hier te
//    verzinnen.
//
#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QChar>
#include <QList>

// Eén geparste FLL-statusregel (handleiding sectie 5.1).
// Voorbeeld ruwe string: L | U | 01FF6 | + | F | 67FC | 0120 | FFFD | 007D | 03
struct FllStatus {
    QChar   state;              // L=Locked, U=Unlocked, H=Holdover, D=Disabled
    QChar   alarmLatch;         // U/B/T/H/.  (latched sinds laatste 'A' commando)
    quint16 dacValue = 0;       // 0x0000-0x3FFF, VCXO-afstemspanning
    QChar   freqAdjSign;        // +/-/=/.
    QChar   freqAdjSize;        // C=coarse (stap 16), F=fine (stap 1), .
    quint16 freqReadout = 0;    // nominaal 0x6800 (=160.000.000 pulsen/16s)
    quint16 sampleCounter = 0;  // huidige teller binnen de averaging-cyclus
    qint16  accumFreqDiff = 0;  // two's complement, betekenis hangt af van M01/M02
    quint16 timestamp = 0;      // vrij lopende 16-bit teller, +1 per 16s
    quint16 holdoverCounter = 0;
    bool    valid = false;
};

class FllLink : public QObject {
    Q_OBJECT
public:
    explicit FllLink(QObject *parent = nullptr);

    // Opent de seriële poort naar het VE2ZAZ-board. Baudrate/framing liggen
    // vast door de hardware (handleiding sectie 5/7): 4800 8N1, geen flow control.
    bool open(const QString &portName);
    void close();

    enum class Phase { Acquisition, SteadyState };
    Phase phase() const { return m_phase; }

signals:
    void statusUpdated(const FllStatus &status);
    void lockAcquired();   // vuurt precies één keer: de EERSTE bevestigde lock
    void lockLost();       // vuurt als een eerder bevestigde lock wegvalt

private slots:
    void onReadyRead();

private:
    void sendCommand(const QString &cmd);
    void beginAcquisitionPhase();
    void switchToSteadyStatePhase();
    void handleLine(const QByteArray &line);
    static bool parseStatus(const QByteArray &line, FllStatus &out);

    QSerialPort m_port;
    QByteArray  m_rxBuffer;

    Phase m_phase = Phase::Acquisition;
    int   m_consecutiveLocked = 0;
    bool  m_haveAcquiredLock = false;

    // --- Instellingen die de acquisitiesnelheid bepalen -----------------------
    // S = averaging cycle size (aantal 16s-samples per DAC-update).
    // M = averaging mode (01=Voting, 02=Summing).
    static constexpr const char *kAcquisitionMode  = "M02";   // Summing: sneller lock bij kleine S (4.4.3.1)
    static constexpr const char *kAcquisitionCycle = "S000A"; // 10 samples = 160s/cyclus (handleiding's eigen aanbeveling, 4.4.5)
    static constexpr const char *kSteadyStateMode  = "M01";   // Voting: beste nauwkeurigheid bij grote S (4.4.3.2)
    static constexpr const char *kSteadyStateCycle = "S0200"; // 512 samples (handleiding's eigen voorbeeldwaarde, 4.4.5)
    static constexpr int kLockConfirmCycles = 5;               // pas vertrouwen na 5 opeenvolgende "L"-cycli (~80s in acquisitiefase)
};
