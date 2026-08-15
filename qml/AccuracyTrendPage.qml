import QtQuick 2.15

// AccuracyTrendPage.qml — pagina 7: nauwkeurigheids-trend (Δf/f), met een
// readout-rij, een zoom-schakelaar (venster in minuten, 0=alles), een
// logaritmische TrendChart en een bijschrift dat de meetmethode uitlegt.

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

    property real windowMinutes: 60

    function fmtE(v) {
        if (v === undefined || v === null || isNaN(v)) return "—"
        if (v === 0) return "0.0E+00"
        var av = Math.abs(v)
        var exp = Math.floor(Math.log(av) / Math.LN10)
        var mantissa = v / Math.pow(10, exp)
        var expStr = (exp >= 0 ? "+" : "-") + (Math.abs(exp) < 10 ? "0" : "") + Math.abs(exp)
        return mantissa.toFixed(1) + "E" + expStr
    }

    function avgInWindow(winSeconds) {
        var pts = gpsdoModel.accHistory
        if (!pts || pts.length === 0) return NaN
        var now = Date.now() / 1000
        var from = winSeconds > 0 ? (now - winSeconds) : -1
        var sum = 0, n = 0
        for (var i = 0; i < pts.length; i++) {
            if (pts[i].t >= from) { sum += pts[i].v; n++ }
        }
        return n > 0 ? sum / n : NaN
    }

    function avgSinceLock() {
        if (!gpsdoModel.lockStartEpoch || gpsdoModel.lockStartEpoch <= 0) return NaN
        var pts = gpsdoModel.accHistory
        if (!pts || pts.length === 0) return NaN
        var sum = 0, n = 0
        for (var i = 0; i < pts.length; i++) {
            if (pts[i].t >= gpsdoModel.lockStartEpoch) { sum += pts[i].v; n++ }
        }
        return n > 0 ? sum / n : NaN
    }

    function latestValue() {
        var pts = gpsdoModel.accHistory
        if (!pts || pts.length === 0) return NaN
        return pts[pts.length - 1].v
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
                text: "Nauwkeurigheid — trend"
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
                    label: "NU (16S)"; value: page.fmtE(page.latestValue())
                }
                DataTile {
                    width: (readoutRow.width - 2 * readoutRow.spacing) / 3
                    height: readoutRow.height
                    valuePixelSize: 16
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "GEM. IN VENSTER"; value: page.fmtE(page.avgInWindow(page.windowMinutes * 60))
                }
                DataTile {
                    width: (readoutRow.width - 2 * readoutRow.spacing) / 3
                    height: readoutRow.height
                    valuePixelSize: 16
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "SINDS LOCK"; value: page.fmtE(page.avgSinceLock())
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
                    { label: "30m", value: 30 },
                    { label: "1u", value: 60 },
                    { label: "3u", value: 180 },
                    { label: "6u", value: 360 },
                    { label: "Alles", value: 0 }
                ]
                selectedValue: page.windowMinutes
                onSelected: (v) => page.windowMinutes = v
            }

            TrendChart {
                id: chart
                width: parent.width
                height: parent.height - 24 * page.uiScale - readoutRow.height - zoom.height - caption.height - 4 * (8 * page.uiScale)
                uiScale: page.uiScale
                points: gpsdoModel.accHistory
                windowSeconds: page.windowMinutes * 60
                yMode: "log"
                yMinLog: 1e-11
                yMaxLog: 1e-8
                lineColor: page.colIce
                fillColor: page.colIce
                gridColor: "#3a3f52"
                labelColor: page.colInkDim
                valueFormatter: function (v) { return page.fmtE(v) }
            }

            Text {
                id: caption
                width: parent.width
                wrapMode: Text.WordWrap
                text: "Δf/f uit VE2ZAZ freq.-uitlezing t.o.v. 0x6800, gemiddeld via geaccumuleerd verschil — vereist averaging-modus M02 (Summing)."
                color: page.colInkDim
                font.pixelSize: 10 * page.uiScale
                font.italic: true
            }
        }
    }
}
