import QtQuick 2.15

// BackgroundGlow.qml — hoek-gloed (rust linksboven/onder, ijsblauw
// rechtsboven/onder) + een zwak 40px technisch raster, precies de
// achtergrond-truc uit de HTML-mockup (".device" se radial-gradients).
// Eén keer getekend (geen doorlopende animatie) — goedkoop voor de Pi.

Canvas {
    id: root
    property color colGold: "#f3714f"
    property color colIce: "#80c8ec"

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
        radial(W * 0.06, H * 0.04, root.colGold, 0.10, 0.42)
        radial(W * 0.96, H * 0.08, root.colIce, 0.07, 0.38)
        radial(W * 0.90, H * 0.96, root.colIce, 0.08, 0.42)
        radial(W * 0.04, H * 0.90, root.colGold, 0.06, 0.38)

        ctx.strokeStyle = Qt.rgba(root.colIce.r, root.colIce.g, root.colIce.b, 0.05)
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
