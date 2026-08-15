import QtQuick 2.15

// FllStatePage.qml — pagina 3: de vier status-tiles (Locked/Unlocked/
// Holdover/Disabled, met de huidige staat gemarkeerd), een 2x3-grid met de
// gedetailleerde FLL-velden, en de ruwe VE2ZAZ-statusregel onderaan.

Item {
    id: page

    property real uiScale: 1.0
    property color colBg: "#000000"
    property color colTile: "#1b1e29"
    property color colSurface: "#0d0f14"
    property color colBorder: "#4a5066"
    property color colInk: "#c9cedd"
    property color colInkDim: "#7d8296"
    property color colGold: "#f3714f"
    property color colIce: "#80c8ec"
    property color colLocked: "#22c25e"
    property color colUnlocked: "#c9c15a"
    property color colHoldover: "#d5623b"
    property color colDisabled: "#ea3323"

    Rectangle {
        anchors.fill: parent
        color: page.colBg

        Column {
            anchors.fill: parent
            anchors.margins: 14 * page.uiScale
            spacing: 10 * page.uiScale

            PageTitle {
                width: parent.width
                uiScale: page.uiScale
                colIce: page.colIce
                colGold: page.colGold
                text: "FLL state — VE2ZAZ"
            }

            // --- 4 status-tiles ---------------------------------------------
            Row {
                id: statusRow
                width: parent.width
                height: 62 * page.uiScale
                spacing: 6 * page.uiScale

                Repeater {
                    model: [
                        { state: "L", label: "Locked",   color: page.colLocked,   textOn: "#06251b" },
                        { state: "U", label: "Unlocked", color: page.colUnlocked, textOn: "#2a2708" },
                        { state: "H", label: "Holdover", color: page.colHoldover, textOn: "#2a0f04" },
                        { state: "D", label: "Disabled", color: page.colDisabled, textOn: "#2a0705" }
                    ]
                    delegate: Rectangle {
                        property bool on: gpsdoModel.lockState === modelData.state
                        width: (statusRow.width - 3 * statusRow.spacing) / 4
                        height: statusRow.height
                        radius: 12 * page.uiScale
                        color: on ? modelData.color : page.colTile
                        border.width: 2 * page.uiScale
                        border.color: modelData.color
                        opacity: on ? 1.0 : 0.4
                        Behavior on opacity { NumberAnimation { duration: 250 } }
                        Behavior on color { ColorAnimation { duration: 250 } }

                        Column {
                            anchors.centerIn: parent
                            spacing: 2 * page.uiScale
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.state
                                color: on ? modelData.textOn : page.colInk
                                font.pixelSize: 18 * page.uiScale
                                font.bold: true
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: modelData.label
                                color: on ? modelData.textOn : page.colInkDim
                                font.pixelSize: 9 * page.uiScale
                                font.letterSpacing: 1
                            }
                        }
                    }
                }
            }

            // --- 2x3 osc-grid ------------------------------------------------
            Item {
                id: oscGrid
                width: parent.width
                // 24*uiScale = PageTitle's vaste hoogte (zie PageTitle.qml) —
                // rechtstreeks als constante gebruikt i.p.v. een verborgen
                // tweede PageTitle-instantie erbij te zetten.
                height: parent.height - 24 * page.uiScale - statusRow.height - rawStrip.height - 4 * (10 * page.uiScale)

                property real tileSpacing: 10 * page.uiScale
                property real tileWidth: (width - tileSpacing) / 2
                property real tileHeight: (height - tileSpacing) / 3

                DataTile {
                    x: 0; y: 0
                    width: oscGrid.tileWidth; height: oscGrid.tileHeight
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "DAC WAARDE"; value: gpsdoModel.dacHex
                }
                DataTile {
                    x: oscGrid.tileWidth + oscGrid.tileSpacing; y: 0
                    width: oscGrid.tileWidth; height: oscGrid.tileHeight
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "FREQ. UITLEZING"; value: gpsdoModel.freqReadoutHex
                }
                DataTile {
                    x: 0; y: oscGrid.tileHeight + oscGrid.tileSpacing
                    width: oscGrid.tileWidth; height: oscGrid.tileHeight
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "SAMPLE-TELLER"; value: gpsdoModel.sampleCounterHex
                }
                DataTile {
                    x: oscGrid.tileWidth + oscGrid.tileSpacing; y: oscGrid.tileHeight + oscGrid.tileSpacing
                    width: oscGrid.tileWidth; height: oscGrid.tileHeight
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "Δ FREQ (ACC.)"; value: gpsdoModel.accumDiffHex
                }
                DataTile {
                    x: 0; y: 2 * (oscGrid.tileHeight + oscGrid.tileSpacing)
                    width: oscGrid.tileWidth; height: oscGrid.tileHeight
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "HOLDOVER-TELLER"; value: gpsdoModel.holdoverHex
                }
                DataTile {
                    x: oscGrid.tileWidth + oscGrid.tileSpacing; y: 2 * (oscGrid.tileHeight + oscGrid.tileSpacing)
                    width: oscGrid.tileWidth; height: oscGrid.tileHeight
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "ALARM LATCH"; value: gpsdoModel.alarmText
                }
            }

            // --- ruwe statusregel --------------------------------------------
            Rectangle {
                id: rawStrip
                width: parent.width
                height: 36 * page.uiScale
                radius: 10 * page.uiScale
                color: page.colSurface
                border.width: 1
                border.color: page.colBorder

                Text {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.margins: 12 * page.uiScale
                    elide: Text.ElideRight
                    text: gpsdoModel.rawFllLine.length > 0 ? gpsdoModel.rawFllLine : "—"
                    color: page.colIce
                    font.family: "monospace"
                    font.pixelSize: 13 * page.uiScale
                }
            }
        }
    }

}
