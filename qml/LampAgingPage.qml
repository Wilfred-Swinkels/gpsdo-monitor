import QtQuick 2.15

// LampAgingPage.qml — pagina 8: veroudering van de lampspanning (Rb-lamp),
// lineaire auto-scaled TrendChart (i.p.v. logaritmisch zoals nauwkeurig-
// heid), zoom in dagen. Los van de live/instant lampspanning op Overview:
// dit is een apart lange-termijn-model, bedoeld om ~1 sample/uur te
// verzamelen zodra de MCP3426-ADC bekabeld is (nu nog leeg).

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

    property real windowDays: 7

    function fmtV(v) {
        if (v === undefined || v === null || isNaN(v)) return "—"
        return v.toFixed(3) + " V"
    }

    function pointsInWindow(winSeconds) {
        var pts = gpsdoModel.lampHistory
        if (!pts || pts.length === 0) return []
        var now = Date.now() / 1000
        var from = winSeconds > 0 ? (now - winSeconds) : -1
        var out = []
        for (var i = 0; i < pts.length; i++) if (pts[i].t >= from) out.push(pts[i])
        return out
    }

    function avgInWindow(winSeconds) {
        var pts = page.pointsInWindow(winSeconds)
        if (pts.length === 0) return NaN
        var sum = 0
        for (var i = 0; i < pts.length; i++) sum += pts[i].v
        return sum / pts.length
    }

    function latestValue() {
        var pts = gpsdoModel.lampHistory
        if (!pts || pts.length === 0) return NaN
        return pts[pts.length - 1].v
    }

    function rangeText(winSeconds) {
        var pts = page.pointsInWindow(winSeconds)
        if (pts.length === 0) return "—"
        var vMin = Infinity, vMax = -Infinity
        for (var i = 0; i < pts.length; i++) {
            if (pts[i].v < vMin) vMin = pts[i].v
            if (pts[i].v > vMax) vMax = pts[i].v
        }
        return vMin.toFixed(2) + "–" + vMax.toFixed(2) + " V"
    }

    Rectangle {
        anchors.fill: parent
        color: page.colBg

        Column {
            anchors.fill: parent
            anchors.margins: 14 * page.uiScale
            spacing: 8 * page.uiScale

            PageTitle {
                width: parent.width
                uiScale: page.uiScale
                colIce: page.colIce
                colGold: page.colGold
                text: "Lampspanning — veroudering"
            }

            Row {
                id: readoutRow
                width: parent.width
                height: 54 * page.uiScale
                spacing: 8 * page.uiScale

                DataTile {
                    width: (readoutRow.width - 2 * readoutRow.spacing) / 3
                    height: readoutRow.height
                    valuePixelSize: 16
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "NU"; value: page.fmtV(page.latestValue())
                }
                DataTile {
                    width: (readoutRow.width - 2 * readoutRow.spacing) / 3
                    height: readoutRow.height
                    valuePixelSize: 16
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "GEM. IN VENSTER"; value: page.fmtV(page.avgInWindow(page.windowDays * 86400))
                }
                DataTile {
                    width: (readoutRow.width - 2 * readoutRow.spacing) / 3
                    height: readoutRow.height
                    valuePixelSize: 14
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "BEREIK"; value: page.rangeText(page.windowDays * 86400)
                }
            }

            ZoomToggle {
                id: zoom
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colIce: page.colIce
                colInkDim: page.colInkDim
                options: [
                    { label: "1d", value: 1 },
                    { label: "7d", value: 7 },
                    { label: "30d", value: 30 },
                    { label: "Alles", value: 0 }
                ]
                selectedValue: page.windowDays
                onSelected: (v) => page.windowDays = v
            }

            TrendChart {
                id: chart
                width: parent.width
                height: parent.height - 24 * page.uiScale - readoutRow.height - zoom.height - caption.height - 4 * (8 * page.uiScale)
                uiScale: page.uiScale
                points: gpsdoModel.lampHistory
                windowSeconds: page.windowDays * 86400
                yMode: "linear"
                lineColor: page.colGold
                fillColor: page.colGold
                gridColor: "#3a3f52"
                labelColor: page.colInkDim
                valueFormatter: function (v) { return v.toFixed(2) }
                timeFormatter: function (t) {
                    var d = new Date(t * 1000)
                    return Qt.formatDate(d, "dd-MM")
                }
            }

            Text {
                id: caption
                width: parent.width
                wrapMode: Text.WordWrap
                text: "Los van de live lampspanning op Overview — dit is een apart lange-termijn-model (±1 sample/uur), bewust geïsoleerd van lock-search-jitter."
                color: page.colInkDim
                font.pixelSize: 10 * page.uiScale
                font.italic: true
            }
        }
    }
}
