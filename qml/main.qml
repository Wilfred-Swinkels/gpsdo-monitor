import QtQuick 2.15
import QtQuick.Window 2.15

// main.qml — LCARS-stijl kaderwerk (header/rail/footer + swipebare
// pagina's) rond de 8 pagina's van de UI. Bewust een gewone Window i.p.v.
// ApplicationWindow, en anchors i.p.v. Column/Layout (geen impliciete-
// hoogte-circulariteit), en geen QtQuick.Controls (paginanavigatie hieronder
// is met een kale ListView opgebouwd i.p.v. SwipeView) — zie de projectbrief
// voor de volledige motivatie.
//
// Kleuren/lettertypen 1-op-1 overgenomen uit de bestaande HTML-mockup
// (gpsdo_lcars_720.html). TODO: Antonio/Oswald/Ubuntu Mono zijn nog niet
// lokaal gebundeld voor de Pi — voorlopig systeem-fonts.
//
// BELANGRIJK — schaling: het ontwerp is getekend op 720x720, maar onder
// eglfs (fullscreen KMS) negeert Qt de hier opgegeven width/height meestal
// toch en maakt het venster exact zo groot als het scherm dat de Pi
// detecteert. `uiScale` (= werkelijke breedte / 720) compenseert dat: alle
// afmetingen in dit bestand en in de losse paginabestanden zijn hiermee
// vermenigvuldigd. Zie eerdere versies van dit bestand / de projectbrief
// voor de volledige geschiedenis (afgekapte titel, overlappende tegels,
// een per ongeluk verzonnen verticale balk links — allemaal hierdoor oa.
// veroorzaakt en opgelost).
//
// Paginanavigatie: elke pagina is een los .qml-bestand, hier ingeladen via
// een Component + Loader per ListView-delegate (i.p.v. alle 8 pagina's
// altijd volledig te instantiëren) — de niet-zichtbare pagina's bestaan dus
// pas zodra je er voorbij swipet. Voetnoot-stippen onderin volgen/sturen de
// huidige pagina, exact zoals de HTML-mockup dat met .dot/.dot.active deed.

Window {
    id: root
    width: 720
    height: 720
    visible: true
    title: "GPSDO Monitor"
    color: colBg

    readonly property real uiScale: width / 720

    // --- LCARS-kleuren, uit de bestaande HTML-mockup overgenomen -----------
    readonly property color colBg: "#000000"
    readonly property color colSurface: "#0d0f14"
    readonly property color colTile: "#1b1e29"
    readonly property color colTile2: "#242838"
    readonly property color colBorder: "#4a5066"
    readonly property color colInk: "#c9cedd"
    readonly property color colInkDim: "#7d8296"
    readonly property color colGold: "#f3714f"
    readonly property color colIce: "#80c8ec"
    readonly property color colLocked: "#22c25e"
    readonly property color colUnlocked: "#c9c15a"
    readonly property color colHoldover: "#d5623b"
    readonly property color colDisabled: "#ea3323"

    function stateColor(state) {
        switch (state) {
        case "L": return colLocked
        case "U": return colUnlocked
        case "H": return colHoldover
        case "D": return colDisabled
        default:  return colInkDim
        }
    }

    // --- Live klok (header) — UTC, met datum eronder, zoals de mockup's
    // #clockTime/#clockDate (die gebruiken expliciet getUTCHours() e.d.,
    // niet de lokale systeemtijd van de Pi).
    function pad2(n) { return (n < 10 ? "0" : "") + n }
    property date headerNow: new Date()
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.headerNow = new Date()
    }
    readonly property string clockTimeText:
        root.pad2(root.headerNow.getUTCHours()) + ":" + root.pad2(root.headerNow.getUTCMinutes()) + ":" + root.pad2(root.headerNow.getUTCSeconds())
    readonly property string clockDateText:
        root.headerNow.getUTCFullYear() + "-" + root.pad2(root.headerNow.getUTCMonth() + 1) + "-" + root.pad2(root.headerNow.getUTCDate()) + " UTC"

    // --- Achtergrond: hoek-gloed + fijn technisch grid + sterrenveld -------
    // Zelfde "dual-direction gradient"-truc als de HTML-mockup: rust-gloed
    // linksboven, ijsblauwe gloed rechtsboven/rechtsonder/linksonder, plus
    // een zwak 40px-raster. Statisch getekend (geen doorlopende animatie),
    // dus goedkoop voor de Pi.
    BackgroundGlow {
        anchors.fill: parent
        colGold: root.colGold
        colIce: root.colIce
    }
    StarField {
        anchors.fill: parent
        starCount: 30
    }

    // --- Header ------------------------------------------------------------
    // Herbouwd naar de ECHTE mockup-opzet (".hdr"/".hdr-elbow"/".hdr-title"/
    // ".hdr-clock") — dit was eerder abusievelijk een vólle gouden balk met
    // donkere tekst; de mockup is juist een DONKERE balk (surface-kleur) met
    // een klein donker "elleboog"-blokje met een gouden accent, en een
    // tweekleurige titel (GPSDO in goud, MONITOR in ijsblauw).
    //
    // Kanttekening: CSS's "border-radius:32px 0 0 0" rondt alleen de
    // linkerbovenhoek af; QML's Rectangle.radius kent geen per-hoek-
    // varianten zonder een nieuwere Qt6-minorversie-API te gebruiken (bewust
    // vermeden, zie projectbrief) — hier daarom een bescheiden radius op
    // alle 4 hoeken van het elleboog-blokje i.p.v. exact 1 hoek. Verder:
    // de mockup's "SIM data"-knipperlampje is een demo-only indicator (er
    // wordt daar nep-data gesimuleerd) en dus bewust weggelaten — deze app
    // praat met echte hardware.
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 58 * root.uiScale
        color: colSurface
        z: 1

        // --- elleboog: klein donker blokje met een goud->ijs accentbalkje ---
        Rectangle {
            id: hdrElbow
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 52 * root.uiScale
            radius: 16 * root.uiScale
            color: colTile
            border.width: 2 * root.uiScale
            border.color: colBorder

            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                anchors.topMargin: 8 * root.uiScale
                anchors.bottomMargin: 8 * root.uiScale
                width: 3 * root.uiScale
                gradient: Gradient {
                    GradientStop { position: 0.0; color: colGold }
                    GradientStop { position: 1.0; color: colIce }
                }
            }
        }

        // --- titel: "GPSDO" (goud) + "MONITOR" (ijsblauw) --------------------
        Row {
            id: titleRow
            anchors.left: hdrElbow.right
            anchors.leftMargin: 20 * root.uiScale
            anchors.right: hdrClock.left
            anchors.rightMargin: 12 * root.uiScale
            anchors.verticalCenter: parent.verticalCenter
            clip: true

            Text {
                text: "GPSDO"
                color: colGold
                font.pixelSize: 22 * root.uiScale
                font.bold: true
                font.letterSpacing: 1 * root.uiScale
            }
            Text {
                text: "MONITOR"
                color: colIce
                font.pixelSize: 22 * root.uiScale
                font.bold: true
                font.letterSpacing: 1 * root.uiScale
            }
        }

        // --- klok: UTC-tijd + datum, rechts, eigen donker vlak ---------------
        Rectangle {
            id: hdrClock
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 150 * root.uiScale
            color: colTile
            border.width: 2 * root.uiScale
            border.color: colBorder

            Column {
                anchors.right: parent.right
                anchors.rightMargin: 14 * root.uiScale
                anchors.verticalCenter: parent.verticalCenter
                spacing: 1 * root.uiScale

                Text {
                    anchors.right: parent.right
                    text: root.clockTimeText
                    color: colIce
                    font.family: "monospace"
                    font.bold: true
                    font.pixelSize: 18 * root.uiScale
                }
                Text {
                    anchors.right: parent.right
                    text: root.clockDateText
                    color: colInkDim
                    font.family: "monospace"
                    font.pixelSize: 9 * root.uiScale
                }
            }
        }
    }

    // --- Header-rail: dunne horizontale gradiëntlijn onder de header -----
    Rectangle {
        id: headerRail
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 3 * root.uiScale
        z: 1
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0;  color: colGold }
            GradientStop { position: 0.55; color: colIce }
            GradientStop { position: 1.0;  color: colBorder }
        }
    }

    // --- Footer: paginadots ------------------------------------------------
    Rectangle {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 32 * root.uiScale
        color: colSurface
        z: 1

        Row {
            anchors.centerIn: parent
            spacing: 10 * root.uiScale

            Repeater {
                model: pagesList.count
                delegate: Rectangle {
                    required property int index
                    width: (index === pagesList.currentIndex ? 13 : 9) * root.uiScale
                    height: width
                    radius: width / 2
                    color: index === pagesList.currentIndex ? root.colIce : root.colTile2
                    border.width: 1
                    border.color: index === pagesList.currentIndex ? root.colIce : root.colBorder
                    Behavior on width { NumberAnimation { duration: 150 } }

                    MouseArea {
                        anchors.margins: -8
                        anchors.fill: parent
                        onClicked: pagesList.currentIndex = index
                    }
                }
            }
        }
    }

    // --- Swipebare pagina's --------------------------------------------
    ListView {
        id: pagesList
        anchors.top: headerRail.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: footer.top
        z: 1
        orientation: ListView.Horizontal
        snapMode: ListView.SnapOneItem
        highlightRangeMode: ListView.StrictlyEnforceRange
        highlightMoveDuration: 200
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        model: 8
        delegate: Item {
            required property int index
            width: pagesList.width
            height: pagesList.height

            Loader {
                anchors.fill: parent
                sourceComponent: root.pageComponents[index]
            }
        }
    }

    // Elke pagina als los Component (lazy — pas gebouwd zodra je er
    // voorbij swipet), met dezelfde kleuren/uiScale-props als de tegels.
    property var pageComponents: [
        overviewComp, skyplotComp, fllStateComp, gpsFixComp,
        chronoAnalogComp, chronoDigitalComp, accTrendComp, lampAgingComp
    ]

    Component {
        id: overviewComp
        OverviewPage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colTile2: root.colTile2
            colBorder: root.colBorder; colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
            stateColorFn: root.stateColor
        }
    }
    Component {
        id: skyplotComp
        SkyplotPage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colBorder: root.colBorder
            colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
        }
    }
    Component {
        id: fllStateComp
        FllStatePage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colSurface: root.colSurface
            colBorder: root.colBorder; colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
            colLocked: root.colLocked; colUnlocked: root.colUnlocked
            colHoldover: root.colHoldover; colDisabled: root.colDisabled
        }
    }
    Component {
        id: gpsFixComp
        GpsFixPage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colSurface: root.colSurface
            colBorder: root.colBorder; colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
        }
    }
    Component {
        id: chronoAnalogComp
        ChronoAnalogPage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colBorder: root.colBorder
            colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
        }
    }
    Component {
        id: chronoDigitalComp
        ChronoDigitalPage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colBorder: root.colBorder
            colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
        }
    }
    Component {
        id: accTrendComp
        AccuracyTrendPage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colBorder: root.colBorder
            colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
        }
    }
    Component {
        id: lampAgingComp
        LampAgingPage {
            uiScale: root.uiScale
            colBg: root.colBg; colTile: root.colTile; colBorder: root.colBorder
            colInk: root.colInk; colInkDim: root.colInkDim
            colGold: root.colGold; colIce: root.colIce
        }
    }
}
