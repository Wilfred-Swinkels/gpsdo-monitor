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

Window {
    id: root
    width: 720
    height: 720
    visible: true
    title: "GPSDO Monitor"
    color: colBg

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
        height: 64
        color: colGold

        Rectangle {
            // LCARS-elleboog: afgeronde linkerhoek van de header
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 64
            radius: 32
            color: colGold
        }

        Text {
            id: titleText
            anchors.left: parent.left
            anchors.leftMargin: 88
            anchors.right: clockLabel.left
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            // Bewust begrensd tot clockLabel.left + elide: zonder dit liep de
            // titel bij een lange tekst gewoon door tot-ie de klok overlapte
            // (gezien op de echte hardware — vaste breedte lost het op).
            elide: Text.ElideRight
            text: "GPSDO MONITOR"
            color: colBg
            font.pixelSize: 26
            font.bold: true
            font.letterSpacing: 2
        }

        Text {
            id: clockLabel
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            text: root.clockText
            color: colBg
            font.family: "monospace"
            font.pixelSize: 24
            font.bold: true
        }
    }

    // --- Linker rail -----------------------------------------------------
    Rectangle {
        id: rail
        anchors.left: parent.left
        anchors.top: header.bottom
        anchors.bottom: footer.top
        width: 20
        color: colIce
    }

    // --- Footer: paginadots (nu nog maar 1 pagina: Overview) -------------
    Rectangle {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 32
        color: colSurface

        Row {
            anchors.centerIn: parent
            spacing: 10

            Rectangle {
                width: 10; height: 10; radius: 5
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
