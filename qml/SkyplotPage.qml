import QtQuick 2.15

// SkyplotPage.qml — pagina 2: polair diagram van alle zichtbare satellieten
// (UBX NAV-SVINFO via GpsdoModel.satellites), gekleurd naar CN0
// (signaalsterkte). Canvas-based, zelfde polaire projectie als de SVG-versie
// in de HTML-mockup (elevatie 90°=midden, 0°=rand; noord boven).

Item {
    id: page

    property real uiScale: 1.0
    property color colBg: "#000000"
    property color colTile: "#1b1e29"
    property color colBorder: "#4a5066"
    property color colInk: "#c9cedd"
    property color colInkDim: "#7d8296"
    property color colGold: "#f3714f"
    property color colIce: "#80c8ec"

    function cn0Color(cn0) {
        var stops = ["#0d1f28", "#164a5c", "#1f7a93", "#3fa8c4", "#80c8ec"]
        var idx = Math.min(stops.length - 1, Math.floor((cn0 / 50) * stops.length))
        return stops[Math.max(0, idx)]
    }

    Rectangle {
        anchors.fill: parent
        color: page.colBg

        Column {
            anchors.fill: parent
            anchors.margins: 14 * page.uiScale
            spacing: 8 * page.uiScale

            PageTitle {
                id: pageTitle
                width: parent.width
                uiScale: page.uiScale
                colIce: page.colIce
                colGold: page.colGold
                text: "GPS skyplot — UBX NAV-SVINFO"
            }

            Canvas {
                id: sky
                width: parent.width
                height: parent.height - pageTitle.height - legend.height - meta.height - 3 * (8 * page.uiScale)
                property var sats: gpsdoModel.satellites

                onPaint: {
                    var ctx = getContext("2d")
                    var W = width, H = height
                    ctx.clearRect(0, 0, W, H)
                    var cx = W / 2, cy = H / 2
                    var R = Math.min(W, H) / 2 - 18 * page.uiScale

                    function polar(el, az) {
                        var rad = R * (1 - el / 90)
                        var a = (az - 90) * Math.PI / 180
                        return [cx + rad * Math.cos(a), cy + rad * Math.sin(a)]
                    }

                    ctx.strokeStyle = "#2a2d3a"
                    ctx.lineWidth = 1
                    ;[0, 30, 60, 90].forEach(function (elev) {
                        var rad = R * (1 - elev / 90)
                        ctx.beginPath(); ctx.arc(cx, cy, rad, 0, 2 * Math.PI); ctx.stroke()
                    })

                    ctx.fillStyle = "#7d8296"
                    ctx.font = "bold " + (13 * page.uiScale) + "px sans-serif"
                    ctx.textAlign = "center"
                    var dirs = ["N", "E", "S", "W"]
                    for (var i = 0; i < 4; i++) {
                        var p = polar(-10, i * 90)
                        ctx.fillText(dirs[i], p[0], p[1] + 4 * page.uiScale)
                    }

                    var sats = sky.sats || []
                    for (var s = 0; s < sats.length; s++) {
                        var sat = sats[s]
                        var pos = polar(sat.elevationDeg, sat.azimuthDeg)
                        var r = (7 + (sat.cno / 50) * 6) * page.uiScale
                        ctx.beginPath()
                        ctx.arc(pos[0], pos[1], r, 0, 2 * Math.PI)
                        ctx.fillStyle = page.cn0Color(sat.cno)
                        ctx.fill()
                        ctx.strokeStyle = "#000000"
                        ctx.lineWidth = 1.5
                        ctx.stroke()

                        ctx.fillStyle = "#ffffff"
                        ctx.strokeStyle = "#000000"
                        ctx.lineWidth = 2
                        ctx.font = "bold " + (9 * page.uiScale) + "px sans-serif"
                        ctx.textAlign = "center"
                        ctx.strokeText(String(sat.svid), pos[0], pos[1] + 3 * page.uiScale)
                        ctx.fillText(String(sat.svid), pos[0], pos[1] + 3 * page.uiScale)
                    }
                }

                onSatsChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }

            Row {
                id: legend
                width: parent.width
                height: 16 * page.uiScale
                spacing: 8 * page.uiScale

                Text {
                    text: "zwak"
                    color: page.colInkDim
                    font.pixelSize: 10 * page.uiScale
                    anchors.verticalCenter: parent.verticalCenter
                }
                Rectangle {
                    width: parent.width - 100 * page.uiScale
                    height: 7 * page.uiScale
                    anchors.verticalCenter: parent.verticalCenter
                    radius: height / 2
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "#0d1f28" }
                        GradientStop { position: 0.3; color: "#164a5c" }
                        GradientStop { position: 0.55; color: "#1f7a93" }
                        GradientStop { position: 0.8; color: "#3fa8c4" }
                        GradientStop { position: 1.0; color: page.colIce }
                    }
                }
                Text {
                    text: "sterk (CN0)"
                    color: page.colInkDim
                    font.pixelSize: 10 * page.uiScale
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            Row {
                id: meta
                width: parent.width
                height: 16 * page.uiScale

                Text {
                    width: parent.width / 2
                    text: gpsdoModel.satsVisible + " satellieten in beeld"
                    color: page.colInkDim
                    font.pixelSize: 12 * page.uiScale
                    font.family: "monospace"
                }
                Text {
                    width: parent.width / 2
                    horizontalAlignment: Text.AlignRight
                    text: gpsdoModel.satsUsed + " gebruikt in fix"
                    color: page.colInkDim
                    font.pixelSize: 12 * page.uiScale
                    font.family: "monospace"
                }
            }
        }
    }
}
