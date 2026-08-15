import QtQuick 2.15

// OverviewPage.qml — de eerste echte pagina: lock-hero, accuracy-strip,
// lamp-veroudering-strip en een 2x3 grid met de 6 kerncijfers.
//
// Layout met expliciete x/y/width/height i.p.v. Column/GridLayout (zie
// main.qml voor de motivatie: geen impliciete-hoogte-circulariteit).
//
// De 6 grid-tegels volgen bewust exact de eerder afgesproken indeling uit
// de HTML-mockup/projectbrief (screen 1 "Overview"): lampspanning,
// xtal-spanning, sats gebruikt, SNR sats, fix-type, DAC-waarde.
//
// `uiScale` komt van main.qml (werkelijke schermbreedte / 720) — alle
// afmetingen hieronder zijn hiermee vermenigvuldigd zodat de pagina er ook
// verhoudingsgewijs goed uitziet als het scherm niet exact 720x720 blijkt
// te zijn. Zie main.qml voor de volledige uitleg.
//
// Kleuren worden van main.qml doorgegeven i.p.v. hier hardcoded, zodat er
// straks maar één plek is om het palet aan te passen.

Item {
    id: page

    property real uiScale: 1.0

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

        // --- Lock hero — alleen state, geen DAC-waarde meer hier -----------
        // DAC-waarde staat (zoals afgesproken) in de 6-tegel-grid hieronder,
        // niet ook nog los in de hero — dat was dubbelop.
        Rectangle {
            id: lockHero
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10 * page.uiScale
            height: 118 * page.uiScale
            radius: 8 * page.uiScale
            color: page.colTile
            border.width: 3 * page.uiScale
            border.color: page.stateColorFn(gpsdoModel.lockState)

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20 * page.uiScale
                anchors.right: parent.right
                anchors.rightMargin: 20 * page.uiScale
                anchors.top: parent.top
                anchors.topMargin: 16 * page.uiScale
                elide: Text.ElideRight
                text: gpsdoModel.lockLabel
                color: page.stateColorFn(gpsdoModel.lockState)
                font.pixelSize: 36 * page.uiScale
                font.bold: true
                font.letterSpacing: 2 * page.uiScale
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 20 * page.uiScale
                anchors.right: parent.right
                anchors.rightMargin: 20 * page.uiScale
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 14 * page.uiScale
                elide: Text.ElideRight
                text: gpsdoModel.lockSubText
                color: page.colInkDim
                font.pixelSize: 13 * page.uiScale
            }
        }

        // --- Accuracy-strip (Δf/f) — dubbele hoogte, grotere tekst ----------
        Rectangle {
            id: accStrip1
            anchors.top: lockHero.bottom
            anchors.topMargin: 8 * page.uiScale
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10 * page.uiScale
            height: 88 * page.uiScale
            radius: 6 * page.uiScale
            color: page.colTile2

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16 * page.uiScale
                anchors.verticalCenter: parent.verticalCenter
                text: "Δf/f"
                color: page.colInkDim
                font.pixelSize: 17 * page.uiScale
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16 * page.uiScale
                anchors.left: parent.left
                anchors.leftMargin: 90 * page.uiScale
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                anchors.verticalCenter: parent.verticalCenter
                text: gpsdoModel.accuracyText
                color: page.colGold
                font.family: "monospace"
                font.pixelSize: 28 * page.uiScale
                font.bold: true
            }
        }

        // --- Lampspanning-veroudering-strip ---------------------------------
        // Per ontwerp: laatste 1x/uur-sample + trend (V/dag), losstaand van
        // de live lampspanning die ook in de grid hieronder staat. Sparkline/
        // trendcijfer/ringbuffer zijn nog niet gebouwd (staat in Volgende
        // stap) — voorlopig alleen de placeholder-tekst.
        Rectangle {
            id: accStrip2
            anchors.top: accStrip1.bottom
            anchors.topMargin: 6 * page.uiScale
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10 * page.uiScale
            height: 88 * page.uiScale
            radius: 6 * page.uiScale
            color: page.colTile2

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16 * page.uiScale
                anchors.right: parent.right
                anchors.rightMargin: 90 * page.uiScale
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: "Lampspanning — veroudering"
                color: page.colInkDim
                font.pixelSize: 16 * page.uiScale
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16 * page.uiScale
                anchors.verticalCenter: parent.verticalCenter
                text: "—"
                color: page.colInkDim
                font.family: "monospace"
                font.pixelSize: 26 * page.uiScale
            }
        }

        // --- 2x3 grid: lampspanning, xtal-spanning, sats gebruikt, --------
        //     SNR sats, fix-type, DAC-waarde (vaste volgorde, zie boven) ---
        Item {
            id: grid
            anchors.top: accStrip2.bottom
            anchors.topMargin: 8 * page.uiScale
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10 * page.uiScale

            property real tileSpacing: 8 * page.uiScale
            property real tileWidth: (width - tileSpacing) / 2
            property real tileHeight: (height - 2 * tileSpacing) / 3

            StatTile {
                x: 0
                y: 0
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "LAMP"
                value: gpsdoModel.lampVoltageText
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: 0
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "XTAL"
                value: gpsdoModel.xtalVoltageText
            }
            StatTile {
                x: 0
                y: grid.tileHeight + grid.tileSpacing
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "SATS GEBRUIKT"
                value: gpsdoModel.hasGpsData ? gpsdoModel.satsUsed : "—"
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: grid.tileHeight + grid.tileSpacing
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "SNR SATS"
                value: gpsdoModel.snrAvgText
            }
            StatTile {
                x: 0
                y: 2 * (grid.tileHeight + grid.tileSpacing)
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "FIX TYPE"
                value: gpsdoModel.fixTypeText
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: 2 * (grid.tileHeight + grid.tileSpacing)
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "DAC"
                value: gpsdoModel.dacHex
            }
        }
    }
}
