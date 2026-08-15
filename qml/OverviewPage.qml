import QtQuick 2.15

// OverviewPage.qml — de eerste echte pagina: lock-hero, accuracy-strip,
// lamp-/xtal-placeholder en een 2x3 grid met GPS/FLL-kerngetallen.
// Layout met expliciete x/y/width/height i.p.v. Column/GridLayout (zie
// main.qml voor de motivatie: geen impliciete-hoogte-circulariteit).
//
// Kleuren worden van main.qml doorgegeven i.p.v. hier hardcoded, zodat er
// straks maar één plek is om het palet aan te passen.

Item {
    id: page

    // Kleuren, doorgegeven vanuit main.qml.
    property color colBg: "#000000"
    property color colTile: "#1b1e29"
    property color colTile2: "#242838"
    property color colBorder: "#4a5066"
    property color colInk: "#c9cedd"
    property color colInkDim: "#7d8296"
    property color colGold: "#f3714f"
    property color colIce: "#80c8ec"
    // Functie state("L"/"U"/"H"/"D"/...) -> kleur, doorgegeven vanuit main.qml.
    property var stateColorFn: function (state) { return colInkDim }

    Rectangle {
        anchors.fill: parent
        color: page.colBg

        // --- Lock hero (118px) --------------------------------------------
        Rectangle {
            id: lockHero
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            height: 118
            radius: 8
            color: page.colTile
            border.width: 3
            border.color: page.stateColorFn(gpsdoModel.lockState)

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.top: parent.top
                anchors.topMargin: 16
                text: gpsdoModel.lockLabel
                color: page.stateColorFn(gpsdoModel.lockState)
                font.pixelSize: 40
                font.bold: true
                font.letterSpacing: 3
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 14
                anchors.right: dacText.left
                anchors.rightMargin: 16
                elide: Text.ElideRight
                text: gpsdoModel.lockSubText
                color: page.colInkDim
                font.pixelSize: 14
            }

            Text {
                id: dacText
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                text: gpsdoModel.dacHex
                color: page.colIce
                font.family: "monospace"
                font.pixelSize: 22
            }
        }

        // --- Accuracy-strip (2x 44px) --------------------------------------
        Rectangle {
            id: accStrip1
            anchors.top: lockHero.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            height: 44
            radius: 6
            color: page.colTile2

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "Δf/f"
                color: page.colInkDim
                font.pixelSize: 14
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: gpsdoModel.accuracyText
                color: page.colGold
                font.family: "monospace"
                font.pixelSize: 18
                font.bold: true
            }
        }

        Rectangle {
            id: accStrip2
            anchors.top: accStrip1.bottom
            anchors.topMargin: 6
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10
            height: 44
            radius: 6
            color: page.colTile2

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: "Lamp / Xtal (nog niet bekabeld)"
                color: page.colInkDim
                font.pixelSize: 13
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                text: gpsdoModel.lampVoltageText + "  /  " + gpsdoModel.xtalVoltageText
                color: page.colInkDim
                font.family: "monospace"
                font.pixelSize: 16
            }
        }

        // --- 2x3 stat-tile grid -----------------------------------------
        Item {
            id: grid
            anchors.top: accStrip2.bottom
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10

            property real tileSpacing: 8
            property real tileWidth: (width - tileSpacing) / 2
            property real tileHeight: (height - 2 * tileSpacing) / 3

            StatTile {
                x: 0
                y: 0
                width: grid.tileWidth
                height: grid.tileHeight
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "SATS GEBRUIKT"
                value: gpsdoModel.hasGpsData ? gpsdoModel.satsUsed : "—"
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: 0
                width: grid.tileWidth
                height: grid.tileHeight
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "SATS ZICHTBAAR"
                value: gpsdoModel.hasGpsData ? gpsdoModel.satsVisible : "—"
            }
            StatTile {
                x: 0
                y: grid.tileHeight + grid.tileSpacing
                width: grid.tileWidth
                height: grid.tileHeight
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "GEM. SNR"
                value: gpsdoModel.snrAvgText
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: grid.tileHeight + grid.tileSpacing
                width: grid.tileWidth
                height: grid.tileHeight
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "FIX TYPE"
                value: gpsdoModel.fixTypeText
            }
            StatTile {
                x: 0
                y: 2 * (grid.tileHeight + grid.tileSpacing)
                width: grid.tileWidth
                height: grid.tileHeight
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "HDOP"
                value: gpsdoModel.hdopText
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: 2 * (grid.tileHeight + grid.tileSpacing)
                width: grid.tileWidth
                height: grid.tileHeight
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "FLL STATUS"
                value: gpsdoModel.lockState
            }
        }
    }
}
