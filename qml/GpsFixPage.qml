import QtQuick 2.15

// GpsFixPage.qml — pagina 4: fix type / HDOP / sats gebruikt als 3 tiles,
// gevolgd door een balkendiagram met de top-8 satellieten gesorteerd op
// CN0 (signaalsterkte), zelfde ".bar-chart"/".bar-row" opbouw als de
// HTML-mockup.

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

    property var topSats: []

    function cn0Color(cn0) {
        var stops = ["#0d1f28", "#164a5c", "#1f7a93", "#3fa8c4", "#80c8ec"]
        var idx = Math.min(stops.length - 1, Math.floor((cn0 / 50) * stops.length))
        return stops[Math.max(0, idx)]
    }

    function updateTopSats() {
        var sats = (gpsdoModel.satellites || []).slice()
        sats.sort(function (a, b) { return b.cno - a.cno })
        page.topSats = sats.slice(0, 8)
    }

    Connections {
        target: gpsdoModel
        function onGpsChanged() { page.updateTopSats() }
    }
    Component.onCompleted: page.updateTopSats()

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Column {
            anchors.fill: parent
            anchors.margins: 14 * page.uiScale
            spacing: 10 * page.uiScale

            PageTitle {
                width: parent.width
                uiScale: page.uiScale
                colIce: page.colIce
                colGold: page.colGold
                text: "GPS fix — u-blox LEA-5T"
            }

            // --- 3 tiles -------------------------------------------------
            Row {
                id: tilesRow
                width: parent.width
                height: 62 * page.uiScale
                spacing: 8 * page.uiScale

                DataTile {
                    width: (tilesRow.width - 2 * tilesRow.spacing) / 3
                    height: tilesRow.height
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "FIX TYPE"; value: gpsdoModel.hasGpsData ? gpsdoModel.fixTypeText : "—"
                }
                DataTile {
                    width: (tilesRow.width - 2 * tilesRow.spacing) / 3
                    height: tilesRow.height
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "HDOP"; value: gpsdoModel.hasGpsData ? gpsdoModel.hdopText : "—"
                }
                DataTile {
                    width: (tilesRow.width - 2 * tilesRow.spacing) / 3
                    height: tilesRow.height
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "SATS GEBRUIKT"; value: gpsdoModel.hasGpsData ? gpsdoModel.satsUsed : "—"
                }
            }

            Text {
                id: subtitle
                width: parent.width
                text: "Signaalsterkte per satelliet (CN0)"
                color: page.colInkDim
                font.pixelSize: 12 * page.uiScale
                font.letterSpacing: 1
                font.bold: true
            }

            // --- balkendiagram --------------------------------------------
            Column {
                id: barChart
                width: parent.width
                height: parent.height - 24 * page.uiScale - tilesRow.height - subtitle.height - 3 * (10 * page.uiScale)
                spacing: 4 * page.uiScale

                Repeater {
                    model: page.topSats
                    delegate: Item {
                        width: barChart.width
                        height: Math.max(14 * page.uiScale, (barChart.height - 7 * barChart.spacing) / 8)

                        Row {
                            anchors.fill: parent
                            spacing: 8 * page.uiScale

                            Text {
                                width: 34 * page.uiScale
                                anchors.verticalCenter: parent.verticalCenter
                                text: "G" + modelData.svid
                                color: page.colInk
                                font.family: "monospace"
                                font.pixelSize: 12 * page.uiScale
                                font.bold: true
                            }

                            Rectangle {
                                id: track
                                width: parent.width - 34 * page.uiScale - 46 * page.uiScale - 2 * 8 * page.uiScale
                                height: 10 * page.uiScale
                                anchors.verticalCenter: parent.verticalCenter
                                radius: height / 2
                                color: page.colSurface
                                border.width: 1
                                border.color: page.colBorder
                                clip: true

                                Rectangle {
                                    height: parent.height
                                    width: parent.width * Math.min(1, modelData.cno / 50)
                                    radius: height / 2
                                    color: page.cn0Color(modelData.cno)
                                }
                            }

                            Text {
                                width: 46 * page.uiScale
                                anchors.verticalCenter: parent.verticalCenter
                                horizontalAlignment: Text.AlignRight
                                text: modelData.cno.toFixed(0) + " dB"
                                color: page.colInkDim
                                font.family: "monospace"
                                font.pixelSize: 11 * page.uiScale
                            }
                        }
                    }
                }

                Text {
                    visible: page.topSats.length === 0
                    width: barChart.width
                    text: "geen satellietdata"
                    color: page.colInkDim
                    font.pixelSize: 12 * page.uiScale
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
