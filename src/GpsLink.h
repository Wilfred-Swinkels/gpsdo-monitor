#pragma once
//
// GpsLink.h — seriële link + UBX-parser voor de u-blox LEA-5T (5e-generatie,
// timing-variant), voor de skyplot en de GPS-fix-pagina van de UI.
//
// Waarom UBX en niet alleen NMEA: NMEA GSV geeft per satelliet alleen ID,
// elevatie, azimuth en SNR, en fix-informatie zit verspreid over meerdere
// zinnen (GGA/GSA) met beperkte precisie. UBX geeft dezelfde en meer data
// in een compact binair formaat rechtstreeks uit één bron per boodschap.
//
// Waarom NAV-SVINFO en niet NAV-SAT: dit is een 5e-generatie u-blox-module
// (protocolversie ~7). NAV-SAT bestaat pas vanaf de M8-generatie
// (protocolversie 15+) — op deze module is NAV-SVINFO (0x01 0x30) de juiste,
// beschikbare boodschap voor per-satelliet elevatie/azimuth/CN0. Zie ook de
// projectbrief en de u-blox 5 Protocol Specification
// (u-blox5_Protocol_Specifications(GPS.G5-X-07036).pdf).
//
// Voor fix-type/DOP/aantal-sats is om dezelfde generatie-reden gekozen voor
// NAV-SOL (0x01 0x06, geeft o.a. gpsFix, flags, numSV, pDOP) en NAV-DOP
// (0x01 0x04, geeft de losse hDOP) in plaats van het modernere NAV-PVT (pas
// vanaf protocolversie 15).
//
// Byte-layout van de drie gebruikte berichten is, naast de protocolspec,
// gecontroleerd tegen een tweede, onafhankelijke bron: de struct-definities
// in de open-source ROS-driver github.com/GAVLab/ublox
// (include/ublox/ublox_structures.h) — komt exact overeen.
//
//  UBX-framing (alle UBX-berichten, ongeacht klasse): 0xB5 0x62 | class(1) |
//  id(1) | length(2, little-endian, alleen payload) | payload(length bytes)
//  | CK_A(1) | CK_B(1). Checksum = 8-bit Fletcher over class+id+length+
//  payload (dus NIET over de syncbytes zelf).
//
//  NAV-SVINFO (0x01 0x30) payload:
//    iTOW(u4) numCh(u1) globalFlags(u1) reserved2(u2)
//    gevolgd door numCh × 12 bytes: chn(u1) svid(u1) flags(u1) quality(u1)
//    cno(u1) elev(i1, graden) azim(i2, graden) prRes(i4)
//    flags-bits die wij gebruiken: bit0=svUsed (meegeteld in de fix),
//    bit4=unhealthy.
//
//  NAV-SOL (0x01 0x06) payload (52 bytes): iTOW(u4) fTOW(i4) week(i2)
//    gpsFix(u1) flags(u1) ecefX/Y/Z(i4×3) pAcc(u4) ecefVX/VY/VZ(i4×3)
//    sAcc(u4) pDOP(u2) reserved1(u1) numSV(u1) reserved2(u4)
//    gpsFix: 0=geen fix, 1=alleen dead-reckoning, 2=2D, 3=3D,
//    4=GPS+dead-reckoning gecombineerd, 5=alleen tijd.
//    flags-bit0 = GPSfixOK (fix daadwerkelijk bruikbaar).
//    pDOP is geschaald ×0.01 (ruwe waarde / 100 = werkelijke DOP).
//
//  NAV-DOP (0x01 0x04) payload (18 bytes): iTOW(u4) gDOP(u2) pDOP(u2)
//    tDOP(u2) vDOP(u2) hDOP(u2) nDOP(u2) eDOP(u2) — alle DOP-waarden ×0.01.
//
//  NAV-POSLLH (0x01 0x02) payload (28 bytes): iTOW(u4) lon(i4) lat(i4)
//    height(i4) hMSL(i4) hAcc(u4) vAcc(u4). lon/lat zijn ×1e-7 graden
//    (WGS84), height/hMSL in mm — hier alleen lon/lat gebruikt. Toegevoegd
//    voor de sterrenbeelden-overlay op de skyplot-pagina: RA/Dec naar
//    Alt/Az omrekenen vereist de waarnemerspositie, en dit is de simpelste
//    UBX-boodschap die dat rechtstreeks geeft (i.p.v. zelf ECEF->geodetisch
//    om te rekenen uit NAV-SOL's ecefX/Y/Z).
//
// Wat deze driver NIET doet, bewust:
//  - Geen UBX-CFG-PRT (baudrate/protocol van de UART zelf herconfigureren):
//    de module accepteert UBX-invoer sowieso ongeacht de output-instelling,
//    dus dit is niet nodig om NAV-SVINFO/NAV-SOL/NAV-DOP aan te zetten.
//  - Geen CFG-CFG ("save to BBR/flash"): de CFG-MSG-aanpassingen hieronder
//    zijn RAM-only en worden bij elke open() opnieuw gestuurd — bewust
//    dezelfde aanpak als bij FllLink, geen permanente module-instellingen
//    vanuit deze klasse.
//  - Geen Time Mode / Survey-In / Fixed Mode configuratie (CFG-TMODE): dat
//    hoort pas bij een vaste opstelplek van de Pi, zie projectbrief
//    "Volgende stap" — expliciet een latere, aparte actie.
//  - NMEA-uitvoer wordt niet uitgezet: de parser scant de buffer op de
//    0xB5 0x62-syncbytes en negeert al het overige, en NMEA is 7-bit ASCII
//    (0x00-0x7F) dus 0xB5 kan daar sowieso nooit per ongeluk in voorkomen —
//    er is dus geen interferentie tussen NMEA- en UBX-uitvoer op dezelfde poort.

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QList>

struct GpsSatellite {
    quint8 channel = 0;
    quint8 svid = 0;
    quint8 cno = 0;             // carrier-to-noise, dBHz (signaalsterkte)
    qint8  elevationDeg = 0;    // graden, 0-90
    qint16 azimuthDeg = 0;      // graden, 0-360
    bool   usedInFix = false;
    bool   unhealthy = false;
};

struct GpsFix {
    quint8 fixType = 0;         // 0=geen,1=DR,2=2D,3=3D,4=GPS+DR,5=alleen tijd
    bool   fixOk = false;       // NAV-SOL flags-bit0 (GPSfixOK)
    quint8 numSatellites = 0;
    double hdop = 0.0;          // uit NAV-DOP
    double pdop = 0.0;          // uit NAV-SOL
    bool   valid = false;       // minstens 1x een geldig bericht ontvangen

    // Positie uit NAV-POSLLH — nodig voor de sterrenbeelden-overlay op de
    // skyplot-pagina (RA/Dec -> Alt/Az vereist de waarnemerspositie).
    // Losse velden i.p.v. hergebruik van de ECEF-XYZ in NAV-SOL: NAV-POSLLH
    // geeft lengte-/breedtegraad direct, geen eigen ECEF->geodetisch-
    // conversie nodig.
    double latitudeDeg = 0.0;
    double longitudeDeg = 0.0;
    bool   hasPosition = false;
};

class GpsLink : public QObject {
    Q_OBJECT
public:
    explicit GpsLink(QObject *parent = nullptr);

    // 9600 8N1, vast voor de LEA-5T (zie projectbrief). Stuurt na het openen
    // de CFG-MSG-commando's die NAV-SVINFO/NAV-SOL/NAV-DOP periodiek aanzetten.
    bool open(const QString &portName);
    void close();

signals:
    void satellitesUpdated(const QList<GpsSatellite> &satellites);
    void fixUpdated(const GpsFix &fix);
    void errorOccurred(const QString &message);

private slots:
    void onReadyRead();

private:
    void enablePeriodicMessages();
    static QByteArray buildFrame(quint8 msgClass, quint8 msgId, const QByteArray &payload);

    void processBuffer();
    void dispatch(quint8 msgClass, quint8 msgId, const QByteArray &payload);
    void handleNavSvinfo(const QByteArray &payload);
    void handleNavSol(const QByteArray &payload);
    void handleNavDop(const QByteArray &payload);
    void handleNavPosllh(const QByteArray &payload);

    QSerialPort m_port;
    QByteArray  m_rxBuffer;
    GpsFix      m_lastFix; // NAV-SOL, NAV-DOP en NAV-POSLLH vullen elk een deel; hier samengevoegd

    // Bovengrens voor een geloofwaardige payload-lengte — puur defensief,
    // zodat een toevallig foutief gedetecteerde sync niet leidt tot
    // eindeloos wachten op data die nooit komt. De grootste boodschap die
    // wij verwachten is NAV-SVINFO met veel kanalen (ruim onder 500 bytes).
    static constexpr int kMaxPlausiblePayload = 2000;
};
