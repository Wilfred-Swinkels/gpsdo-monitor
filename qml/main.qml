import QtQuick 2.15
import QtQuick.Window 2.15

// main.qml — LCARS-stijl kaderwerk (header/rail/footer) rond de
// Overview-pagina. Bewust een gewone Window i.p.v. ApplicationWindow (geen
// afhankelijkheid van QtQuick.Controls), en anchors i.p.v. Column/Layout
// (geen impliciete-hoogte-circulariteit) — zie de projectbrief voor de
// volledige motivatie.
//
// Kleuren/lettertypen 1-op-1 overgenomen uit de bestaande HTML-mockup
// (gpsdo_lcars_720.html). TODO: Antonio/Oswald/Ubuntu Mono zijn nog niet
// lokaal gebundeld voor de Pi — voorlopig systeem-fonts.
//
// BELANGRIJK — schaling: het ontwerp is getekend op 720x720, maar onder
// eglfs (fullscreen KMS) negeert Qt de hier opgegeven width/height meestal
// toch en maakt het venster exact zo groot als het scherm dat de Pi
// detecteert. Als dat niet exact 720x720 is, werden voorheen alle
// vast-in-pixels opgegeven marges/lettergroottes verhoudingsgewijs te groot
// (of te klein) voor het echte scherm — dat verklaarde zowel de afgekapte
// header-titel als de overlappende tegel-tekst die op de echte hardware
// zichtbaar waren. `uiScale` (= werkelijke breedte / 720) lost dat op: alle
// afmetingen hieronder en in OverviewPage/StatTile zijn met deze factor
// vermenigvuldigd, zodat het ontwerp verhoudingsgewijs klopt op elk scherm,
// niet alleen op precies 720x720.

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

    // --- Live klok -----------------------------------------------------
    property string clockText: Qt.formatTime(new Date(), "hh:mm:ss")
    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: root.clockText = Qt.formatTime(new Date(), "hh:mm:ss")
    }

    // --- Header ----------------------------------------------------------
    Rectangle {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 64 * root.uiScale
        color: colGold

        Rectangle {
            // LCARS-elleboog: afgeronde linkerhoek van de header
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 64 * root.uiScale
            radius: 32 * root.uiScale
            color: colGold
        }

        Text {
            id: titleText
            anchors.left: parent.left
            anchors.leftMargin: 72 * root.uiScale
            anchors.right: clockLabel.left
            anchors.rightMargin: 12 * root.uiScale
            anchors.verticalCenter: parent.verticalCenter
            // Begrensd tot clockLabel.left + elide, zodat de titel nooit meer
            // de klok kan overlappen (was eerder een bug). Lettergrootte
            // bewust iets kleiner dan de eerste versie (22 i.p.v. 26) zodat
            // "GPSDO MONITOR" ook echt past i.p.v. te moeten eliden.
            elide: Text.ElideRight
            text: "GPSDO MONITOR"
            color: colBg
            font.pixelSize: 22 * root.uiScale
            font.bold: true
            font.letterSpacing: 1 * root.uiScale
        }

        Text {
            id: clockLabel
            anchors.right: parent.right
            anchors.rightMargin: 20 * root.uiScale
            anchors.verticalCenter: parent.verticalCenter
            text: root.clockText
            color: colBg
            font.family: "monospace"
            font.pixelSize: 20 * root.uiScale
            font.bold: true
        }
    }

    // --- Linker rail -----------------------------------------------------
    Rectangle {
        id: rail
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: footer.top
        width: 20 * root.uiScale
        color: colIce
    }

    // --- Footer: paginadots (nu nog maar 1 pagina: Overview) -------------
    Rectangle {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 32 * root.uiScale
        color: colSurface

        Row {
            anchors.centerIn: parent
            spacing: 10 * root.uiScale

            Rectangle {
                width: 10 * root.uiScale
                height: 10 * root.uiScale
                radius: width / 2
                color: colGold
            }
        }
    }

    // --- Inhoud: Overview-pagina ------------------------------------------
    OverviewPage {
        id: overviewPage
        anchors.top: header.bottom
        anchors.left: rail.right
        anchors.right: parent.right
        anchors.bottom: footer.top

        uiScale: root.uiScale
        colBg: root.colBg
        colTile: root.colTile
        colTile2: root.colTile2
        colBorder: root.colBorder
        colInk: root.colInk
        colInkDim: root.colInkDim
        colGold: root.colGold
        colIce: root.colIce
        stateColorFn: root.stateColor
    }
}
