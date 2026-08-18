import QtQuick 2.15

// TemperatureTrendPage.qml — pagina 9: "Base plate"-temperatuur-trend
// (TSic 506F, GPIO17), op verzoek 18-08-2026 ("een grafiek pagina waar ik
// de tijd kan selecteren zoals op het stabiliteits scherm").
//
// Volgt bewust exact hetzelfde patroon als AccuracyTrendPage.qml (readout-
// rij + ZoomToggle + TrendChart + bijschrift), met dezelfde tijdvenster-
// opties (30m/1u/3u/6u/Alles) als de nauwkeurigheid-trendpagina ("het
// stabiliteits scherm") — maar met een LINEAIRE Y-as i.p.v. logaritmisch
// (temperatuur heeft geen orde-grootte-bereik zoals Δf/f), naar analogie
// van LampAgingPage.qml. Plot gpsdoModel.tempHistory rechtstreeks (geen
// aparte "avg"-reeks zoals bij Accuracy — temperatuur is niet gekwantiseerd
// zoals de FLL-freq.-uitlezing, dus middelen is hier niet nodig).

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

    function fmtT(v) {
        if (v === undefined || v === null || isNaN(v)) return "—"
        return v.toFixed(1) + "°C"
    }

    function pointsInWindow(winSeconds) {
        var pts = gpsdoModel.tempHistory
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
        var pts = gpsdoModel.tempHistory
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
        return vMin.toFixed(1) + "–" + vMax.toFixed(1) + "°C"
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Column {
            anchors.fill: parent
            anchors.margins: 14 * page.uiScale
            spacing: 8 * page.uiScale

            PageTitle {
                width: parent.width
                uiScale: page.uiScale
                colIce: page.colIce
                colGold: page.colGold
                text: "Base plate — trend"
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
                    label: "NU"; value: page.fmtT(page.latestValue())
                }
                DataTile {
                    width: (readoutRow.width - 2 * readoutRow.spacing) / 3
                    height: readoutRow.height
                    valuePixelSize: 16
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "GEM. IN VENSTER"; value: page.fmtT(page.avgInWindow(page.windowMinutes * 60))
                }
                DataTile {
                    width: (readoutRow.width - 2 * readoutRow.spacing) / 3
                    height: readoutRow.height
                    valuePixelSize: 14
                    uiScale: page.uiScale; colTile: page.colTile; colBorder: page.colBorder
                    colInkDim: page.colInkDim; colIce: page.colIce
                    label: "BEREIK"; value: page.rangeText(page.windowMinutes * 60)
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
                points: gpsdoModel.tempHistory
                windowSeconds: page.windowMinutes * 60
                yMode: "linear"
                lineColor: page.colIce
                fillColor: page.colIce
                gridColor: "#3a3f52"
                labelColor: page.colInkDim
                valueFormatter: function (v) { return v.toFixed(1) + "°C" }
            }

            Text {
                id: caption
                width: parent.width
                wrapMode: Text.WordWrap
                text: "TSic 506F ZACwire-sensor op GPIO17 — temperatuur van de bodemplaat, bedoeld om later te correleren met Δf/f-drift van de Rb-bron."
                color: page.colInkDim
                font.pixelSize: 10 * page.uiScale
                font.italic: true
            }
        }
    }
}
