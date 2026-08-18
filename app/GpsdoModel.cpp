// GpsdoModel.cpp
//
// Zie GpsdoModel.h voor het waarom van de aanpak. Deze file bevat alleen
// "vertaal"-logica (ruwe FllStatus/GpsFix/GpsSatellite -> weergave-strings
// en -lijsten), geen I/O.

#include "GpsdoModel.h"

#include <QtMath>
#include <QDateTime>
#include <cmath>

GpsdoModel::GpsdoModel(QObject *parent) : QObject(parent) {
}

void GpsdoModel::attachFllLink(FllLink *link) {
    connect(link, &FllLink::statusUpdated, this, &GpsdoModel::onFllStatus);
    connect(link, &FllLink::lockAcquired, this, &GpsdoModel::lockAcquired);
    connect(link, &FllLink::lockLost, this, &GpsdoModel::lockLost);
    connect(link, &FllLink::rawLineReceived, this, &GpsdoModel::onRawFllLine);
}

void GpsdoModel::attachGpsLink(GpsLink *link) {
    connect(link, &GpsLink::fixUpdated, this, &GpsdoModel::onGpsFix);
    connect(link, &GpsLink::satellitesUpdated, this, &GpsdoModel::onGpsSatellites);
    connect(link, &GpsLink::surveyInUpdated, this, &GpsdoModel::onSurveyIn);
}

void GpsdoModel::attachTsic506(Tsic506Driver *driver) {
    connect(driver, &Tsic506Driver::temperatureRead, this, &GpsdoModel::onTsicTemperature);
}

void GpsdoModel::attachAdc(Mcp3426Adc *adc) {
    connect(adc, &Mcp3426Adc::voltageRead, this, &GpsdoModel::onAdcVoltage);
}

void GpsdoModel::attachBiteLock(BiteLockDriver *driver) {
    connect(driver, &BiteLockDriver::lockChanged, this, &GpsdoModel::onBiteLockChanged);
}

void GpsdoModel::onFllStatus(const FllStatus &status) {
    m_fllStatus = status;
    emit fllChanged();

    if (status.valid) {
        // Eerste keer dat we "L" zien sinds het starten van de app: dit is
        // het ankerpunt voor de "sinds lock"-uitlezing op de
        // nauwkeurigheid-trendpagina. Geen persistente opslag, dus na een
        // herstart begint dit gewoon opnieuw — eerlijker dan doen alsof we
        // weten wanneer het écht gelockt is (dat kan al veel eerder zijn
        // geweest, vóór deze app draaide).
        if (status.state == QLatin1Char('L') && m_lockStartEpoch == 0.0) {
            m_lockStartEpoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;
        }

        // Lopend gemiddelde, ALLEEN over samples tijdens een ononderbroken
        // "L"-periode — zie computeAccuracyAvgValue()/accuracyAvgText()
        // hieronder voor het waarom (middelt kwantisatieruis weg, maar is
        // zinloos tijdens Unlocked/Holdover, dus dan resetten i.p.v. door-
        // middelen met acquisitie-ruis). Bewust VOOR pushAccHistoryPoint()
        // bijgewerkt, zodat het "avg"-veld van het zo-meteen gepushte punt
        // deze sample al meetelt i.p.v. een sample achterloopt.
        if (status.state == QLatin1Char('L')) {
            if (m_avgAccCount == 0)
                m_avgAccWindowStartEpoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;
            m_avgAccSum += computeAccuracyValue();
            ++m_avgAccCount;
        } else {
            m_avgAccSum = 0.0;
            m_avgAccCount = 0;
            m_avgAccWindowStartEpoch = 0.0;
        }

        pushAccHistoryPoint();
    }
}

void GpsdoModel::onGpsFix(const GpsFix &fix) {
    m_gpsFix = fix;
    emit gpsChanged();
}

void GpsdoModel::onGpsSatellites(const QList<GpsSatellite> &satellites) {
    m_satellites = satellites;
    emit gpsChanged();
}

void GpsdoModel::onSurveyIn(const SurveyInStatus &status) {
    m_surveyIn = status;
    emit surveyInChanged();
}

void GpsdoModel::onRawFllLine(const QByteArray &line) {
    m_rawFllLine = QString::fromLatin1(line).trimmed();
    emit rawFllLineChanged();
}

void GpsdoModel::onTsicTemperature(double celsius, quint16 /*rawValue*/) {
    m_hasTsicData = true;
    m_lastTempC = celsius;
    emit tsicChanged();
    pushTempHistoryPoint(celsius);
}

// channelName is "lamp" of "xtal" — zie de ChannelConfig::name-toewijzing
// in main.cpp (attachAdc()-aanroeper), analoog aan hoe main_test_cli.cpp
// dat al langer standalone deed.
void GpsdoModel::onAdcVoltage(const QString &channelName, double voltage, qint16 /*rawCode*/) {
    if (channelName == QLatin1String("lamp")) {
        m_hasLampData = true;
        m_lampVoltage = voltage;
        pushLampHistoryPoint(voltage);
    } else if (channelName == QLatin1String("xtal")) {
        m_hasXtalData = true;
        m_xtalVoltage = voltage;
    }
    emit adcChanged();
}

void GpsdoModel::onBiteLockChanged(bool locked) {
    m_hasBiteLockData = true;
    m_biteLocked = locked;
    emit biteLockChanged();
}

// --- VE2ZAZ / FllLink -------------------------------------------------

QString GpsdoModel::lockState() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QString(m_fllStatus.state);
}

QString GpsdoModel::lockLabel() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("GEEN DATA");
    switch (m_fllStatus.state.toLatin1()) {
    case 'L': return QStringLiteral("LOCKED");
    case 'U': return QStringLiteral("UNLOCKED");
    case 'H': return QStringLiteral("HOLDOVER");
    case 'D': return QStringLiteral("DISABLED");
    default:  return QStringLiteral("ONBEKEND");
    }
}

QString GpsdoModel::lockSubText() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("Wachten op verbinding met VE2ZAZ...");
    switch (m_fllStatus.state.toLatin1()) {
    case 'L': return QStringLiteral("Frequentie gestabiliseerd op GPS-referentie");
    case 'U': return QStringLiteral("Acquisitie bezig — nog geen lock");
    case 'H': return QStringLiteral("GPS-signaal verloren — houdt laatste DAC-waarde vast");
    case 'D': return QStringLiteral("FLL-regeling uitgeschakeld");
    default:  return QStringLiteral("Onbekende status");
    }
}

QString GpsdoModel::dacHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.dacValue, 4, 16, QLatin1Char('0')).toUpper();
}

double GpsdoModel::computeAccuracyValue() const {
    if (!m_fllStatus.valid)
        return 0.0;
    // freqReadout is nominaal 0x6800 (=160.000.000 pulsen/16s bij 10MHz).
    // Verschil t.o.v. dat nulpunt, als signed 16-bit interpreteren i.v.m.
    // wraparound rond de nominale waarde.
    const qint16 raw = static_cast<qint16>(static_cast<quint16>(m_fllStatus.freqReadout - 0x6800));
    const double deltaFHz = raw / 16.0;      // 1 LSB = 1/16 Hz
    return deltaFHz / 1.0e7;                 // f_nominaal = 10 MHz
}

QString GpsdoModel::accuracyText() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    const double deltaFOverF = computeAccuracyValue();
    const double deltaFHz = deltaFOverF * 1.0e7;
    return QStringLiteral("%1%2 (%3 Hz)")
        .arg(deltaFOverF >= 0 ? QStringLiteral("+") : QString())
        .arg(deltaFOverF, 0, 'e', 2)
        .arg(deltaFHz, 0, 'f', 3);
}

double GpsdoModel::computeAccuracyAvgValue() const {
    if (m_avgAccCount == 0)
        return 0.0;
    return m_avgAccSum / static_cast<double>(m_avgAccCount);
}

// Softwarematig lopend gemiddelde van dezelfde Δf/f-waarde als
// accuracyText(), opgebouwd sinds het begin van de huidige ononderbroken
// "L"-periode. Doel: computeAccuracyValue() is gekwantiseerd in stappen van
// 6,25e-9 (1 LSB van de 16-bit freq.-teller over 16s) — een enkele sample
// kan dus nooit fijner tonen dan dat, en pendelt vaak gewoon tussen 0 en
// ±1 LSB. Door veel van die samples te middelen (klassieke oversampling,
// zelfde principe als een gedithered ADC) verdwijnt die kwantisatiestap
// geleidelijk uit het gemiddelde, en kan de uitlezing na lang genoeg
// middelen (orde grootte uren) dieper dan 1e-9/1e-10 gaan — begrensd door
// de werkelijke ruis van de Rb-bron zelf, niet meer door deze rekenkundige
// vloer. Bewust GEEN wijziging aan de FLL-averaging-mode (M01/M02) — dat
// zou de acquisitie/lock-strategie raken (zie FllLink), dit is puur een
// weergave-berekening op de al binnenkomende statuslijnen.
QString GpsdoModel::accuracyAvgText() const {
    if (m_avgAccCount == 0)
        return QStringLiteral("wacht op ononderbroken lock");

    const double avg = computeAccuracyAvgValue();
    const double windowSec = QDateTime::currentMSecsSinceEpoch() / 1000.0 - m_avgAccWindowStartEpoch;

    QString windowText;
    if (windowSec < 90.0) {
        windowText = QStringLiteral("%1s").arg(qRound(windowSec));
    } else if (windowSec < 3600.0) {
        windowText = QStringLiteral("%1m").arg(qRound(windowSec / 60.0));
    } else {
        const int hours = static_cast<int>(windowSec / 3600.0);
        const int mins = qRound(std::fmod(windowSec, 3600.0) / 60.0);
        windowText = QStringLiteral("%1u%2m").arg(hours).arg(mins, 2, 10, QLatin1Char('0'));
    }

    return QStringLiteral("%1 (%2x): %3%4")
        .arg(windowText)
        .arg(m_avgAccCount)
        .arg(avg >= 0 ? QStringLiteral("+") : QString())
        .arg(avg, 0, 'e', 3);
}

QString GpsdoModel::freqReadoutHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.freqReadout, 4, 16, QLatin1Char('0')).toUpper();
}

QString GpsdoModel::sampleCounterHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.sampleCounter, 4, 16, QLatin1Char('0')).toUpper();
}

QString GpsdoModel::accumDiffHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    const qint16 v = m_fllStatus.accumFreqDiff;
    const quint16 mag = static_cast<quint16>(v < 0 ? -v : v);
    const QString sign = v < 0 ? QStringLiteral("-") : QStringLiteral("+");
    const QString hex = QStringLiteral("%1").arg(mag, 4, 16, QLatin1Char('0')).toUpper();
    return sign + QStringLiteral("0x") + hex;
}

QString GpsdoModel::holdoverHex() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    return QStringLiteral("0x%1").arg(m_fllStatus.holdoverCounter, 4, 16, QLatin1Char('0')).toUpper();
}

QString GpsdoModel::alarmText() const {
    if (!m_fllStatus.valid)
        return QStringLiteral("—");
    if (m_fllStatus.alarmLatch == QLatin1Char('.'))
        return QStringLiteral("Geen alarm");
    return QString(m_fllStatus.alarmLatch);
}

void GpsdoModel::pushAccHistoryPoint() {
    const double epoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;

    QVariantMap point;
    point.insert(QStringLiteral("t"), epoch);
    point.insert(QStringLiteral("v"), computeAccuracyValue());
    // Lopend gemiddelde op dit moment (zie computeAccuracyAvgValue()) —
    // alleen aanwezig tijdens een ononderbroken "L"-periode, want buiten
    // lock is er geen zinnig gemiddelde. AccuracyTrendPage.qml plot deze
    // reeks i.p.v. de rauwe/gekwantiseerde "v"-reeks hierboven.
    if (m_avgAccCount > 0) {
        point.insert(QStringLiteral("avg"), computeAccuracyAvgValue());
    }
    m_accHistory.append(point);

    // Begrens op de laatste 24 uur, met een harde backstop op het aantal
    // punten (defensief, mocht de systeemklok bv. door een NTP-sprong raar
    // springen).
    const double cutoff = epoch - 24.0 * 3600.0;
    while (!m_accHistory.isEmpty() &&
           m_accHistory.constFirst().toMap().value(QStringLiteral("t")).toDouble() < cutoff) {
        m_accHistory.removeFirst();
    }
    while (m_accHistory.size() > 8000) {
        m_accHistory.removeFirst();
    }

    emit accHistoryChanged();
}

// --- LEA-5T / GpsLink ------------------------------------------------

QString GpsdoModel::snrAvgText() const {
    if (m_satellites.isEmpty())
        return QStringLiteral("—");

    int sum = 0;
    int count = 0;
    for (const auto &sat : m_satellites) {
        if (sat.usedInFix) {
            sum += sat.cno;
            ++count;
        }
    }
    if (count == 0)
        return QStringLiteral("—");

    const double avg = static_cast<double>(sum) / count;
    return QStringLiteral("%1 dBHz").arg(avg, 0, 'f', 0);
}

QString GpsdoModel::fixTypeText() const {
    if (!m_gpsFix.valid)
        return QStringLiteral("Geen data");
    if (!m_gpsFix.fixOk)
        return QStringLiteral("Geen fix");
    switch (m_gpsFix.fixType) {
    case 0: return QStringLiteral("Geen fix");
    case 1: return QStringLiteral("Dead-reckoning");
    case 2: return QStringLiteral("2D fix");
    case 3: return QStringLiteral("3D fix");
    case 4: return QStringLiteral("GPS + DR");
    case 5: return QStringLiteral("Alleen tijd");
    default: return QStringLiteral("Onbekend");
    }
}

QString GpsdoModel::hdopText() const {
    if (!m_gpsFix.valid)
        return QStringLiteral("—");
    return QString::number(m_gpsFix.hdop, 'f', 2);
}

QString GpsdoModel::surveyInDurationText() const {
    if (!m_surveyIn.active && !m_surveyIn.valid && m_surveyIn.observations == 0)
        return QStringLiteral("— (survey-in niet actief)");
    const quint32 s = m_surveyIn.durationSec;
    if (s < 3600)
        return QStringLiteral("%1m").arg(s / 60);
    const quint32 hours = s / 3600;
    const quint32 mins = (s % 3600) / 60;
    if (hours < 24)
        return QStringLiteral("%1u%2m").arg(hours).arg(mins, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1d %2u").arg(hours / 24).arg(hours % 24);
}

QString GpsdoModel::surveyInAccuracyText() const {
    if (!m_surveyIn.active && !m_surveyIn.valid && m_surveyIn.observations == 0)
        return QStringLiteral("—");
    // meanVarMm2 is een 3D-variantie in mm² -> stddev in mm is de
    // vierkantswortel, gedeeld door 1000 voor meters.
    const double stddevM = std::sqrt(static_cast<double>(m_surveyIn.meanVarMm2)) / 1000.0;
    return QStringLiteral("±%1 m").arg(stddevM, 0, 'f', 2);
}

// --- TSic 506F / "Base plate"-temperatuur -----------------------------

// Kant-en-klare weergavetekst, inclusief gradensymbool + "C" — op verzoek
// (18-08-2026): "er moet een graden symbool en een C achter de temperatuur
// staan". Eén decimaal, aansluitend bij de gespecificeerde sensor-
// nauwkeurigheid (±0,1K over het 5..45°C-bereik, zie Tsic506Driver.h) —
// meer decimalen tonen zou een precisie suggereren die de sensor niet
// waarmaakt.
QString GpsdoModel::tempText() const {
    if (!m_hasTsicData)
        return QStringLiteral("—");
    return QStringLiteral("%1°C").arg(m_lastTempC, 0, 'f', 1);
}

// Throttled t.o.v. de 10Hz-sample-rate van de sensor (zie Tsic506Driver.h)
// — elke sample pushen zou de 24u-geschiedenis/8000-puntenlimiet (zelfde
// begrenzing als pushAccHistoryPoint()) binnen enkele minuten vollopen.
// Eén punt per ~10s is ruim voldoende voor een temperatuur-trend (die uit
// zijn aard traag verandert) en past ruim binnen de 8000-puntenlimiet over
// 24 uur (24*3600/10 = 8640, dus de tijd-cutoff snoeit iets eerder dan de
// hard-limit hieronder ooit bereikt wordt).
void GpsdoModel::pushTempHistoryPoint(double celsius) {
    const double epoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    if (m_lastTempHistoryPushEpoch != 0.0 && (epoch - m_lastTempHistoryPushEpoch) < 10.0)
        return;
    m_lastTempHistoryPushEpoch = epoch;

    QVariantMap point;
    point.insert(QStringLiteral("t"), epoch);
    point.insert(QStringLiteral("v"), celsius);
    m_tempHistory.append(point);

    const double cutoff = epoch - 24.0 * 3600.0;
    while (!m_tempHistory.isEmpty() &&
           m_tempHistory.constFirst().toMap().value(QStringLiteral("t")).toDouble() < cutoff) {
        m_tempHistory.removeFirst();
    }
    while (m_tempHistory.size() > 8000) {
        m_tempHistory.removeFirst();
    }

    emit tempHistoryChanged();
}

// --- MCP3426 / lamp-/xtal-spanning -------------------------------------

QString GpsdoModel::lampVoltageText() const {
    if (!m_hasLampData)
        return QStringLiteral("—");
    return QStringLiteral("%1 V").arg(m_lampVoltage, 0, 'f', 2);
}

QString GpsdoModel::xtalVoltageText() const {
    if (!m_hasXtalData)
        return QStringLiteral("—");
    return QStringLiteral("%1 V").arg(m_xtalVoltage, 0, 'f', 2);
}

// 1 punt per uur (LPRO-101-veroudering is een trage trend over dagen/weken,
// zie de "Lampspanning — veroudering vs. lock-zoek-jitter"-sectie in de
// projectbrief) — bewust NIET de live/instant spanning, die staat al apart
// in lampVoltageText()/de Overview-tegel. Geen tijd-cutoff (in tegenstelling
// tot pushTempHistoryPoint()/pushAccHistoryPoint()): LampAgingPage.qml biedt
// een "30d"/"Alles"-zoomoptie aan, dus een 24u-cutoff zou die zinloos maken.
// Alleen een ruime harde punten-cap als backstop (20.000 punten bij 1/uur =
// >2 jaar geschiedenis, ruim genoeg om nooit in de praktijk geraakt te
// worden, maar voorkomt onbegrensde groei mocht de app maandenlang
// doordraaien zonder herstart).
void GpsdoModel::pushLampHistoryPoint(double voltage) {
    const double epoch = QDateTime::currentMSecsSinceEpoch() / 1000.0;
    if (m_lastLampHistoryPushEpoch != 0.0 && (epoch - m_lastLampHistoryPushEpoch) < 3600.0)
        return;
    m_lastLampHistoryPushEpoch = epoch;

    QVariantMap point;
    point.insert(QStringLiteral("t"), epoch);
    point.insert(QStringLiteral("v"), voltage);
    m_lampHistory.append(point);

    while (m_lampHistory.size() > 20000) {
        m_lampHistory.removeFirst();
    }

    // adcChanged() (al ge-emit door onAdcVoltage() vóór deze aanroep) dekt
    // lampVoltageText()/xtalVoltageText() al — lampHistory heeft z'n eigen
    // NOTIFY nodig omdat LampAgingPage.qml alleen naar adcChanged luistert
    // via dezelfde property-groep, dus dit is bewust dezelfde emit als de
    // rest van de ADC-properties i.p.v. een apart signaal (geen aparte
    // lampHistoryChanged() nodig, in tegenstelling tot tempHistoryChanged()
    // — dat volgt een ANDER Q_PROPERTY-cluster met een eigen NOTIFY).
}

// --- BITE-lock-detect (LPRO-101 J1-pin 6) op GPIO27 --------------------

// Vertaalt de rauwe boolean naar de UI-teksten die Wilfred bevestigd heeft
// (17-08-2026): LOW (locked) -> "Atomic lock", HIGH (unlocked/opwarmen) ->
// "Warm-up". Vervangt de fix-type-tegel op het Overview-scherm.
QString GpsdoModel::biteLockText() const {
    if (!m_hasBiteLockData)
        return QStringLiteral("—");
    return m_biteLocked ? QStringLiteral("Atomic lock") : QStringLiteral("Warm-up");
}

QVariantList GpsdoModel::satellites() const {
    QVariantList list;
    list.reserve(m_satellites.size());
    for (const auto &s : m_satellites) {
        QVariantMap m;
        m.insert(QStringLiteral("svid"), s.svid);
        m.insert(QStringLiteral("elevationDeg"), s.elevationDeg);
        m.insert(QStringLiteral("azimuthDeg"), s.azimuthDeg);
        m.insert(QStringLiteral("cno"), s.cno);
        m.insert(QStringLiteral("usedInFix"), s.usedInFix);
        m.insert(QStringLiteral("unhealthy"), s.unhealthy);
        list.append(m);
    }
    return list;
}
