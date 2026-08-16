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
//  - NMEA-uitvoer wordt niet uitgezet: de parser scant de buffer op de
//    0xB5 0x62-syncbytes en negeert al het overige, en NMEA is 7-bit ASCII
//    (0x00-0x7F) dus 0xB5 kan daar sowieso nooit per ongeluk in voorkomen —
//    er is dus geen interferentie tussen NMEA- en UBX-uitvoer op dezelfde poort.
//
// Time Mode / Survey-In / Fixed Mode (CFG-TMODE, 0x06 0x1D) — WEL
// geïmplementeerd (startSurveyIn()/disableTimeMode()), maar bewust NIET
// automatisch bij elke open() vanuit DEZE klasse gestuurd zoals de
// periodieke NAV-berichten hierboven: dat zou een lopende, meerdere-uren-
// tot-dagen-durende Survey-In bij elke app-herstart resetten. In plaats
// daarvan regelt `TimeModeSupervisor` (app/TimeModeSupervisor.h) dit één
// laag hoger, en wordt die door main.cpp automatisch aangeroepen zodra
// --gps opgegeven is — geen aparte CLI-vlag (meer) nodig. GpsLink zelf
// blijft dus een dunne, kale protocollaag zonder eigen beleid; zie
// TimeModeSupervisor.h voor de "wanneer/waarom"-logica.
//
//  UBX-CFG-TMODE (0x06 0x1D) payload (28 bytes), bron: officiële u-blox 5
//  Protocol Specification (GPS.G5-X-07036-D), sectie "CFG-TMODE (0x06
//  0x1D)": timeMode(u4, 0=disabled/1=survey-in/2=fixed) fixedPosX/Y/Z(i4×3,
//  ECEF, cm — alleen relevant in Fixed Mode) fixedPosVar(u4, mm², alleen
//  Fixed Mode) svinMinDur(u4, s) svinVarLimit(u4, mm², vereiste 3D-variantie
//  om Survey-In te stoppen). Survey-In bouwt een gewogen gemiddelde van alle
//  geldige 3D-posities op en stopt zodra ZOWEL svinMinDur ALS svinVarLimit
//  bereikt zijn; de ontvanger schakelt daarna zelf automatisch over naar
//  Fixed Mode met die gemeten ECEF-positie — geen handmatige geodetisch->
//  ECEF-conversie nodig vanuit deze driver.
//
//  UBX-TIM-SVIN (0x0D 0x04) payload (28 bytes), zelfde bron, sectie
//  "TIM-SVIN (0x0D 0x04)": dur(u4,s) meanX/Y/Z(i4×3, ECEF cm) meanV(u4,
//  mm²) obs(u4) valid(u1) active(u1) reserved(u2) — voortgangsrapportage
//  tijdens een lopende Survey-In, altijd periodiek aangezet (net als de
//  NAV-berichten) — onschadelijk als timeMode niet op Survey-In staat, dan
//  komt gewoon dur/obs=0, valid/active=0 binnen.
//
// DIAGNOSE (Wilfred, 2026-08-16): op echte hardware NAK't deze LEA-5T
// CFG-TMODE (0x1D), zowel de poll-variant als het Set-commando (zie
// main.cpp) — maar
// Wilfred kreeg dezelfde fysieke module met u-center 8.17 wél in
// Survey-In. Meest waarschijnlijke verklaring: deze "5T"-firmware
// implementeert het NIEUWERE UBX-CFG-TMODE2 (0x06 0x3D, officieel pas
// gedocumenteerd vanaf u-blox 6) i.p.v./naast de legacy 0x1D, en u-center
// detecteert dat automatisch en gebruikt het juiste bericht. open()
// stuurt nu daarom ALTIJD ook een read-only diagnostische poll van
// CFG-TMODE2 (lege payload, net als de bestaande CFG-TMODE-poll) — puur
// om empirisch te bevestigen of dat klopt (ACK/inhoudelijk antwoord i.p.v.
// NAK), VOORDAT er een echte CFG-TMODE2-implementatie (startSurveyIn() etc.
// omzetten naar het andere byte-formaat) gebouwd wordt. Zie
// cfgTmode2RawResponseReceived() hieronder en main.cpp voor de logging.
//
// VERVOLG (Wilfred, zelfde dag): in u-center zelf ziet Wilfred een
// "Time Mode 3"-pagina staan — dat is vermoedelijk UBX-CFG-TMODE3 (0x06
// 0x71), NIET TMODE2. Onderzoek (3 onafhankelijke, onderling matchende
// code-bronnen: ublox_msgs/CfgTMODE3.msg, PX4/GpsDrivers ubx.h/.cpp, en
// python-ubx) bevestigt de 40-byte payload hieronder — LET OP: officieel
// vereist CFG-TMODE3 protocolversie 20+ (de M8P/High-Precision-lijn), dus
// het is onzeker of deze u5-gebaseerde 5T 'm ook daadwerkelijk accepteert;
// u-center kan de pagina gewoon altijd tonen ongeacht of de aangesloten
// ontvanger 'm ondersteunt. Vandaar: OOK hiervan nu een read-only
// diagnostische poll (lege payload, zelfde patroon als TMODE2 hierboven)
// i.p.v. blind aannemen dat dit hét antwoord is. Byte-layout (Set- en
// Poll-Response-payload zijn identiek):
//   version(U1@0) reserved1(U1@1) flags(X2@2, bits0-7=mode
//   0=Disabled/1=Survey-In/2=Fixed, bit8=lla 0=ECEF/1=lat-lon-alt)
//   ecefXOrLat(I4@4, cm of 1e-7deg) ecefYOrLon(I4@8) ecefZOrAlt(I4@12)
//   ecefXOrLatHP(I1@16, 0.1mm/1e-9deg-extensie, -99..99)
//   ecefYOrLonHP(I1@17) ecefZOrAltHP(I1@18) reserved2(U1@19)
//   fixedPosAcc(U4@20, 0.1mm) svinMinDur(U4@24,s) svinAccLimit(U4@28,
//   0.1mm) reserved3(U1[8]@32) — totaal 40 bytes. Zie
//   cfgTmode3RawResponseReceived() hieronder en main.cpp voor de logging.

#include <QObject>
#include <QSerialPort>
#include <QByteArray>
#include <QString>
#include <QStringList>
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

    // ECEF-positie uit NAV-SOL (ecefX/Y/Z, cm) — apart van latitudeDeg/
    // longitudeDeg hierboven, want CFG-TMODE/TIM-SVIN werken zelf ook in
    // ECEF (cm). Nodig voor TimeModeSupervisor's "is de antenne verplaatst"-
    // vergelijking: zo is er geen eigen geodetisch<->ECEF-conversie nodig
    // (en dus geen eigen foutbron) om een verse positie te vergelijken met
    // de eerder opgeslagen gefixeerde ECEF-positie.
    qint32 ecefXcm = 0;
    qint32 ecefYcm = 0;
    qint32 ecefZcm = 0;
    bool   hasEcef = false;
};

// Voortgang van een lopende (of net afgelopen) Survey-In — uit UBX-TIM-SVIN.
// Zie GpsLink.h bovenaan voor de bronvermelding van de byte-layout.
struct SurveyInStatus {
    quint32 durationSec = 0;    // dur — verstreken observatietijd
    qint32  meanXcm = 0;        // meanX — ECEF, cm
    qint32  meanYcm = 0;        // meanY — ECEF, cm
    qint32  meanZcm = 0;        // meanZ — ECEF, cm
    quint32 meanVarMm2 = 0;     // meanV — huidige 3D-variantie van de meting, mm²
    quint32 observations = 0;   // obs — aantal gebruikte metingen
    bool    valid = false;      // valid — positie geldig
    bool    active = false;     // active — Survey-In loopt nog
};

class GpsLink : public QObject {
    Q_OBJECT
public:
    explicit GpsLink(QObject *parent = nullptr);

    // 9600 8N1, vast voor de LEA-5T (zie projectbrief). Stuurt na het openen
    // de CFG-MSG-commando's die NAV-SVINFO/NAV-SOL/NAV-DOP/TIM-SVIN periodiek
    // aanzetten.
    bool open(const QString &portName);
    void close();

    // Start Time Mode Survey-In (CFG-TMODE, timeMode=1) — zie de toelichting
    // bovenaan dit bestand. ONVOORWAARDELIJK: stuurt altijd het commando,
    // ongeacht de huidige staat. Vandaar dat main.cpp dit niet rechtstreeks
    // aanroept, maar via requestAutoSurveyIn() hieronder — gebruik deze
    // functie alleen als je bewust een lopende/afgeronde Survey-In wilt
    // overschrijven (bv. na disableTimeMode(), zie hieronder).
    // minDurationSeconds: minimale Survey-In-duur, ongeacht hoe snel de
    // gevraagde nauwkeurigheid al gehaald wordt (voorkomt een toevallig te
    // vroeg "geluk gehad"-resultaat op een klein aantal fixes).
    // varLimitMm2: vereiste 3D-positievariantie (mm²) om te stoppen — dus
    // kwadraat van de gewenste standaarddeviatie in mm.
    bool startSurveyIn(quint32 minDurationSeconds, quint32 varLimitMm2);

    // Zet Time Mode terug naar Disabled (timeMode=0). Bedoeld voor de
    // "de antenne is verplaatst"-situatie: eerst dit aanroepen, dan
    // requestAutoSurveyIn() (of startSurveyIn() rechtstreeks) om een verse
    // meting te starten — zie main.cpp, --reset-survey-in.
    bool disableTimeMode();

    // Idempotente, "veilige" variant: vraagt EERST de huidige Time Mode-
    // status op bij de module (CFG-TMODE poll request) en start Survey-In
    // ALLEEN als die nog op Disabled staat. Als er al een Survey-In loopt
    // (timeMode=1) of de module al Fixed staat (timeMode=2), gebeurt er
    // niets — dat bestaande resultaat blijft met rust.
    //
    // Waarom dit nodig is: de LEA-5T onthoudt zijn Time Mode-staat zelf in
    // RAM zolang de USB-module stroom houdt — een herstart van gpsdo_app
    // (zonder dat de module zelf stroomloos is geweest) verandert daar
    // niets aan. Door hier altijd eerst te CHECKEN i.p.v. blind te
    // versturen, is het veilig om dit gewoon automatisch bij elke app-start
    // te laten lopen (zie TimeModeSupervisor::begin(), aangeroepen door
    // main.cpp zodra --gps opgegeven is): een al lopende of al afgeronde
    // meting wordt nooit per ongeluk gereset door een simpele app-herstart.
    //
    // Wat DEZE functie (op zichzelf) niet doet: detecteren dat de antenne
    // fysiek verplaatst is terwijl de module al in Fixed Mode staat — deze
    // hardware/Time Mode-generatie berekent in Fixed Mode geen
    // onafhankelijke positie meer om tegen te vergelijken, dus dat kan
    // GpsLink hier niet zelf zien. Die detectie zit een laag hoger, in
    // `TimeModeSupervisor` (app/TimeModeSupervisor.h): die roept, ALS
    // requestAutoSurveyIn() hier een reeds-Fixed status terugmeldt, zelf
    // disableTimeMode() aan om een verse 3D-fix te forceren en te
    // vergelijken met de opgeslagen positie. Handmatig kan ook: zie
    // disableTimeMode() hierboven en main.cpp, --reset-survey-in.
    bool requestAutoSurveyIn(quint32 minDurationSeconds, quint32 varLimitMm2);

    // Zet Time Mode direct op Fixed (timeMode=2) met EXPLICIETE ECEF-
    // coordinaten — voor TimeModeSupervisor's "positie geverifieerd,
    // gewoon terugzetten op de al bekende, opgeslagen positie" pad, zonder
    // daarvoor een nieuwe, uren-tot-dagen-durende Survey-In te hoeven
    // draaien. posVarMm2: aangenomen 3D-variantie van deze positie (mm²) —
    // gebruik de variantie die de oorspronkelijke Survey-In opleverde
    // (TIM-SVIN meanV), niet zomaar 0.
    bool setFixedPosition(qint32 ecefXcm, qint32 ecefYcm, qint32 ecefZcm, quint32 posVarMm2);

signals:
    void satellitesUpdated(const QList<GpsSatellite> &satellites);
    void fixUpdated(const GpsFix &fix);
    void surveyInUpdated(const SurveyInStatus &status);
    // Antwoord op de CFG-TMODE poll request uit requestAutoSurveyIn() (of
    // een eventuele toekomstige handmatige poll) — het ruwe timeMode-veld
    // (0=Disabled/1=Survey-In/2=Fixed), voor diagnostiek/logging.
    void timeModeReported(quint8 timeMode);
    // UBX-ACK-ACK/ACK-NAK (0x05 0x01 / 0x05 0x00) — de module bevestigt of
    // wijst elk CFG-bericht dat WIJ sturen af (zie spec: "Any messages in
    // Class CFG sent to the receiver are acknowledged... or rejected...").
    // Toegevoegd specifiek om CFG-TMODE te kunnen diagnosticeren: als de
    // module Time Mode niet ondersteunt of het commando om wat voor reden
    // dan ook afwijst, komt dat hier als acked=false binnen i.p.v. stil te
    // verdwijnen. msgClass/msgId zijn van het bevestigde/afgewezen bericht
    // (dus bv. 0x06/0x1D voor een CFG-TMODE-commando), niet van deze
    // ACK/NAK zelf.
    void ackReceived(quint8 msgClass, quint8 msgId, bool acked);
    // Antwoord op de MON-VER-poll die open() automatisch verstuurt (zuiver
    // informatief, geen configuratiewijziging) — software-/hardware-
    // versiestring plus de "extension"-regels die de module zelf
    // rapporteert. Toegevoegd om te kunnen VERIFIËREN of deze specifieke
    // module zichzelf als timing-capable identificeert, nadat CFG-TMODE in
    // de praktijk een NAK opleverde op echte hardware — zie main.cpp.
    void versionInfoReceived(const QString &swVersion, const QString &hwVersion,
                              const QStringList &extensions);
    // Antwoord op de diagnostische CFG-TMODE2 (0x06 0x3D)-poll die open()
    // automatisch verstuurt (zie de "DIAGNOSE"-toelichting bovenaan dit
    // bestand) — de RUWE payload-bytes, nog niet geparsed (CFG-TMODE2 heeft
    // een ander byte-formaat dan de legacy CFG-TMODE, zie u-blox M8 spec).
    // Alleen bedoeld om te bevestigen DAT de module inhoudelijk antwoordt
    // (i.p.v. NAK zoals bij 0x1D) — een echte parser komt er pas als dat
    // bevestigd is.
    void cfgTmode2RawResponseReceived(const QByteArray &payload);
    // Zelfde als cfgTmode2RawResponseReceived() hierboven, maar dan voor de
    // diagnostische CFG-TMODE3 (0x06 0x71)-poll — zie de "VERVOLG"-
    // toelichting bovenaan dit bestand.
    void cfgTmode3RawResponseReceived(const QByteArray &payload);
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
    void handleTimSvin(const QByteArray &payload);
    void handleCfgTmodeResponse(const QByteArray &payload);
    void handleAck(const QByteArray &payload, bool acked);
    void handleMonVer(const QByteArray &payload);

    QSerialPort m_port;
    QByteArray  m_rxBuffer;
    GpsFix      m_lastFix; // NAV-SOL, NAV-DOP en NAV-POSLLH vullen elk een deel; hier samengevoegd

    // Staat voor requestAutoSurveyIn(): welke parameters te gebruiken ALS
    // het antwoord op de poll laat zien dat timeMode nog Disabled is. Puur
    // eenmalig — gereset na de eerstvolgende CFG-TMODE-respons, dus een
    // toevallige latere poll (zou hier nooit vandaan komen, maar defensief)
    // triggert niet nog een keer.
    bool    m_autoSurveyInPending = false;
    quint32 m_autoSurveyMinDurationSeconds = 0;
    quint32 m_autoSurveyVarLimitMm2 = 0;

    // Bovengrens voor een geloofwaardige payload-lengte — puur defensief,
    // zodat een toevallig foutief gedetecteerde sync niet leidt tot
    // eindeloos wachten op data die nooit komt. De grootste boodschap die
    // wij verwachten is NAV-SVINFO met veel kanalen (ruim onder 500 bytes).
    static constexpr int kMaxPlausiblePayload = 2000;
};
