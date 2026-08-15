import QtQuick 2.15

// BackgroundGlow.qml — hoek-gloed (rood linksboven/onder, blauw
// rechtsboven/onder) + een zwak 40px technisch raster, precies de
// achtergrond-truc uit de HTML-mockup (".device" se radial-gradients).
// Eén keer getekend (geen doorlopende animatie) — goedkoop voor de Pi.
//
// Bewust FELLERE, apart benoemde rood/blauw-tinten i.p.v. de gedempte
// colGold/colIce-paletkleuren die de rest van de UI gebruikt — op verzoek
// duidelijk zichtbaar, niet subtiel.

Canvas {
    id: root
    property color colGold: "#f3714f"
    property color colIce: "#80c8ec"
    property color colRed: "#e5372b"
    property color colBlue: "#2f6fed"

    onPaint: {
        var ctx = getContext("2d")
        var W = width, H = height
        ctx.clearRect(0, 0, W, H)

        function radial(cx, cy, col, alpha, rFrac) {
            var r = Math.max(W, H) * rFrac
            var g = ctx.createRadialGradient(cx, cy, 0, cx, cy, r)
            g.addColorStop(0, Qt.rgba(col.r, col.g, col.b, alpha))
            g.addColorStop(1, Qt.rgba(col.r, col.g, col.b, 0))
            ctx.fillStyle = g
            ctx.fillRect(0, 0, W, H)
        }
        radial(W * 0.04, H * 0.02, root.colRed, 0.55, 0.62)
        radial(W * 0.98, H * 0.06, root.colBlue, 0.50, 0.58)
        radial(W * 0.94, H * 0.98, root.colBlue, 0.50, 0.62)
        radial(W * 0.02, H * 0.94, root.colRed, 0.45, 0.58)

        ctx.strokeStyle = Qt.rgba(root.colIce.r, root.colIce.g, root.colIce.b, 0.06)
        ctx.lineWidth = 1
        var step = 40
        for (var x = 0; x <= W; x += step) {
            ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, H); ctx.stroke()
        }
        for (var y = 0; y <= H; y += step) {
            ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(W, y); ctx.stroke()
        }
    }

    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
}
