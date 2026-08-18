import QtQuick 2.15

// TrendChart.qml — herbruikbare trendgrafiek (Canvas-based) voor de
// nauwkeurigheid- (log-Y) en lampspanning-veroudering-pagina's (lineaire Y),
// met een gevulde lijn, raster+asaanduidingen en tooltip-op-tik/sleep —
// hetzelfde idee als de SVG-grafieken in de HTML-mockup, maar dan met
// QtQuick Canvas i.p.v. losse SVG-elementen (die had ik moeten laten
// tekenen door DOM-manipulatie, hier tekent Canvas rechtstreeks).
//
// `points` is de QVariantList uit GpsdoModel (accHistory/lampHistory):
// [{t: seconden-sinds-epoch, v: waarde}, ...]. Leeg/te kort -> lege grafiek
// (geen crash, gewoon niets tekenen).

Item {
    id: root

    property var points: []
    property real windowSeconds: 0          // 0 = alles
    property string yMode: "linear"         // "linear" of "log"
    property real yMinLog: 1e-11
    property real yMaxLog: 1e-8
    property color lineColor: "#80c8ec"
    property color fillColor: "#80c8ec"
    property color gridColor: "#3a3f52"
    property color labelColor: "#7d8296"
    property real uiScale: 1.0
    // Alleen relevant voor yMode "log": de Y-positie toont altijd |v| (zie
    // yFracLog), dus een teken-wissel (+ naar - of andersom) is normaal
    // onzichtbaar — lijkt gewoon een duik naar de bodem. Met signAware:true
    // wordt elk lijnsegment gekleurd op het teken van zijn eindpunt, zodat
    // zo'n nul-doorgang zichtbaar wordt als kleurwissel i.p.v. verborgen te
    // blijven. Default false, dus bestaand gedrag (bv. de lineaire
    // lampspanning-pagina) blijft ongewijzigd.
    property bool signAware: false
    property color negativeLineColor: "#f3714f"
    property var valueFormatter: function (v) { return v.toFixed(2) }
    property var timeFormatter: function (t) {
        var d = new Date(t * 1000)
        return Qt.formatTime(d, "hh:mm")
    }

    property var _lastGeom: null

    Canvas {
        id: canvas
        anchors.fill: parent

        onPaint: {
            var ctx = getContext("2d")
            var W = width, H = height
            ctx.clearRect(0, 0, W, H)

            var padL = 46 * root.uiScale, padR = 6 * root.uiScale
            var padT = 6 * root.uiScale, padB = 20 * root.uiScale
            var chartW = Math.max(1, W - padL - padR)
            var chartH = Math.max(1, H - padT - padB)

            var now = Date.now() / 1000
            var pts = root.points
            if (!pts || pts.length < 2) { root._lastGeom = null; return }

            var winS = root.windowSeconds
            var from = winS > 0 ? (now - winS) : pts[0].t
            var visible = []
            for (var i = 0; i < pts.length; i++) if (pts[i].t >= from) visible.push(pts[i])
            if (visible.length < 2) { root._lastGeom = null; return }

            function xPix(t) { return padL + (t - from) / Math.max(1, (now - from)) * chartW }

            var vMin, vMax
            if (root.yMode === "linear") {
                vMin = Infinity; vMax = -Infinity
                for (var j = 0; j < visible.length; j++) {
                    var vv = visible[j].v
                    if (vv < vMin) vMin = vv
                    if (vv > vMax) vMax = vv
                }
                var span = vMax - vMin
                if (span < 1e-9) { var mid = (vMax + vMin) / 2; vMin = mid - 0.1; vMax = mid + 0.1 }
                var m = (vMax - vMin) * 0.15
                vMin -= m; vMax += m
            }
            function yFracLog(v) {
                var av = Math.min(root.yMaxLog, Math.max(root.yMinLog, Math.abs(v)))
                return (Math.log(root.yMaxLog) - Math.log(av)) / (Math.log(root.yMaxLog) - Math.log(root.yMinLog))
            }
            function yPix(v) {
                var frac = root.yMode === "log" ? yFracLog(v) : (1 - (v - vMin) / Math.max(1e-12, (vMax - vMin)))
                return padT + frac * chartH
            }

            // -- raster + Y-labels --
            ctx.font = (10 * root.uiScale) + "px monospace"
            var gridVals = []
            if (root.yMode === "log") {
                var expMin = Math.round(Math.log(root.yMinLog) / Math.LN10)
                var expMax = Math.round(Math.log(root.yMaxLog) / Math.LN10)
                for (var e = expMax; e >= expMin; e--) gridVals.push(Math.pow(10, e))
            } else {
                for (var g = 0; g <= 4; g++) gridVals.push(vMin + (vMax - vMin) * (g / 4))
            }
            for (var k = 0; k < gridVals.length; k++) {
                var gy = yPix(gridVals[k])
                ctx.strokeStyle = root.gridColor
                ctx.setLineDash([2, 4])
                ctx.beginPath(); ctx.moveTo(padL, gy); ctx.lineTo(W - padR, gy); ctx.stroke()
                ctx.setLineDash([])
                ctx.fillStyle = root.labelColor
                ctx.textAlign = "right"
                ctx.fillText(root.valueFormatter(gridVals[k]), padL - 6, gy + 3)
            }

            // -- X-ticks --
            var nTicks = 4
            for (var ti = 0; ti <= nTicks; ti++) {
                var t = from + (now - from) * (ti / nTicks)
                var x = xPix(t)
                ctx.strokeStyle = "#242838"
                ctx.beginPath(); ctx.moveTo(x, padT); ctx.lineTo(x, H - padB); ctx.stroke()
                ctx.fillStyle = root.labelColor
                ctx.textAlign = ti === 0 ? "left" : (ti === nTicks ? "right" : "center")
                ctx.fillText(root.timeFormatter(t), x, H - padB + 14 * root.uiScale)
            }

            // -- gevulde lijn --
            var grad = ctx.createLinearGradient(0, padT, 0, H - padB)
            grad.addColorStop(0, Qt.rgba(root.fillColor.r, root.fillColor.g, root.fillColor.b, 0.22))
            grad.addColorStop(1, Qt.rgba(root.fillColor.r, root.fillColor.g, root.fillColor.b, 0))
            ctx.beginPath()
            for (var p = 0; p < visible.length; p++) {
                var px = xPix(visible[p].t), py = yPix(visible[p].v)
                if (p === 0) ctx.moveTo(px, py); else ctx.lineTo(px, py)
            }
            ctx.lineTo(xPix(visible[visible.length - 1].t), H - padB)
            ctx.lineTo(xPix(visible[0].t), H - padB)
            ctx.closePath()
            ctx.fillStyle = grad
            ctx.fill()

            ctx.lineWidth = 2 * root.uiScale
            ctx.lineJoin = "round"; ctx.lineCap = "round"
            if (root.signAware) {
                for (var q = 1; q < visible.length; q++) {
                    var qx0 = xPix(visible[q - 1].t), qy0 = yPix(visible[q - 1].v)
                    var qx1 = xPix(visible[q].t), qy1 = yPix(visible[q].v)
                    ctx.strokeStyle = visible[q].v < 0 ? root.negativeLineColor : root.lineColor
                    ctx.beginPath()
                    ctx.moveTo(qx0, qy0)
                    ctx.lineTo(qx1, qy1)
                    ctx.stroke()
                }
            } else {
                ctx.beginPath()
                for (var q2 = 0; q2 < visible.length; q2++) {
                    var qx2 = xPix(visible[q2].t), qy2 = yPix(visible[q2].v)
                    if (q2 === 0) ctx.moveTo(qx2, qy2); else ctx.lineTo(qx2, qy2)
                }
                ctx.strokeStyle = root.lineColor
                ctx.stroke()
            }

            var lastPt = visible[visible.length - 1]
            var lx = xPix(lastPt.t), ly = yPix(lastPt.v)
            ctx.beginPath()
            ctx.arc(lx, ly, 4 * root.uiScale, 0, 2 * Math.PI)
            ctx.fillStyle = (root.signAware && lastPt.v < 0) ? root.negativeLineColor : root.lineColor
            ctx.fill()
            ctx.strokeStyle = "#000000"
            ctx.lineWidth = 1.5 * root.uiScale
            ctx.stroke()

            root._lastGeom = { visible: visible, xPix: xPix, yPix: yPix }
        }
    }

    onPointsChanged: canvas.requestPaint()
    onWindowSecondsChanged: canvas.requestPaint()
    onWidthChanged: canvas.requestPaint()
    onHeightChanged: canvas.requestPaint()
    Timer { interval: 3000; running: true; repeat: true; onTriggered: canvas.requestPaint() }

    Rectangle {
        id: tooltip
        visible: false
        radius: 6 * root.uiScale
        color: "#0d0f14"
        border.width: 1
        border.color: root.lineColor
        width: tipText.implicitWidth + 16 * root.uiScale
        height: tipText.implicitHeight + 10 * root.uiScale

        Text {
            id: tipText
            anchors.centerIn: parent
            color: root.lineColor
            font.pixelSize: 11 * root.uiScale
            font.family: "monospace"
        }
    }

    MouseArea {
        anchors.fill: parent
        onPositionChanged: (mouse) => root.showTooltipAt(mouse.x)
        onPressed: (mouse) => root.showTooltipAt(mouse.x)
        onExited: tooltip.visible = false
    }

    function showTooltipAt(x) {
        var geom = root._lastGeom
        if (!geom) return
        var best = null, bestDx = Infinity
        for (var i = 0; i < geom.visible.length; i++) {
            var dx = Math.abs(geom.xPix(geom.visible[i].t) - x)
            if (dx < bestDx) { bestDx = dx; best = geom.visible[i] }
        }
        if (!best) return
        var px = geom.xPix(best.t), py = geom.yPix(best.v)
        tipText.text = root.timeFormatter(best.t) + "  " + root.valueFormatter(best.v)
        tooltip.x = Math.min(root.width - tooltip.width, Math.max(0, px - tooltip.width / 2))
        tooltip.y = Math.max(0, py - tooltip.height - 8 * root.uiScale)
        tooltip.visible = true
    }
}
