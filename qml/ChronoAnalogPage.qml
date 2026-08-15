import QtQuick 2.15

// ChronoAnalogPage.qml — pagina 5: analoge klok (Canvas i.p.v. SVG, zelfde
// tekentechniek als de HTML-mockup: 60 tick-marks, uur/minuut/seconde-
// wijzers, gouden hub), UTC/CEST-knoppen aan weerszijden (i.p.v. een
// samen-gegroepeerde schakelaar in het midden — de knop zelf laat al zien
// welke weergave actief is, dus geen aparte "Weergave: ..."-tekst meer
// nodig), en een decoratieve "stardate"-regel rechts uitgelijnd.

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

    property string tzMode: "CEST"
    property date now: new Date()

    Timer {
        interval: 1000
        running: true
        repeat: true
        onTriggered: page.now = new Date()
    }

    function displayDate() {
        if (page.tzMode === "CEST") {
            var d = new Date(page.now.getTime())
            d.setHours(d.getHours() + 2)
            return d
        }
        return page.now
    }

    function stardateFor(d) {
        var dayStart = new Date(d.getFullYear(), d.getMonth(), d.getDate())
        var dayFrac = (d.getTime() - dayStart.getTime()) / 86400000
        return 40000 + dayFrac * 30
    }

    // Achtergrond bewust transparant — de gedeelde sterrenveld/hoek-gloed-
    // achtergrond zit al achter de swipebare pagina's in main.qml.
    Rectangle {
        anchors.fill: parent
        color: "transparent"

        Column {
            anchors.fill: parent
            anchors.margins: 12 * page.uiScale
            spacing: 6 * page.uiScale

            PageTitle {
                width: parent.width
                uiScale: page.uiScale
                colIce: page.colIce
                colGold: page.colGold
                text: "Chronometer — analoog"
            }

            // --- UTC/CEST-knoppen aan weerszijden i.p.v. samen in het midden ---
            Item {
                id: toggleRow
                width: parent.width
                height: 32 * page.uiScale

                Rectangle {
                    id: utcBtn
                    anchors.left: parent.left
                    height: parent.height
                    width: utcLabel.implicitWidth + 26 * page.uiScale
                    radius: height / 2
                    color: page.tzMode === "UTC" ? page.colIce : page.colTile
                    border.width: 2 * page.uiScale
                    border.color: page.tzMode === "UTC" ? page.colIce : page.colBorder
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        id: utcLabel
                        anchors.centerIn: parent
                        text: "UTC"
                        color: page.tzMode === "UTC" ? "#06232f" : page.colInkDim
                        font.pixelSize: 14 * page.uiScale
                        font.bold: true
                        font.letterSpacing: 1 * page.uiScale
                    }
                    MouseArea { anchors.fill: parent; onClicked: page.tzMode = "UTC" }
                }

                Rectangle {
                    id: cestBtn
                    anchors.right: parent.right
                    height: parent.height
                    width: cestLabel.implicitWidth + 26 * page.uiScale
                    radius: height / 2
                    color: page.tzMode === "CEST" ? page.colIce : page.colTile
                    border.width: 2 * page.uiScale
                    border.color: page.tzMode === "CEST" ? page.colIce : page.colBorder
                    Behavior on color { ColorAnimation { duration: 150 } }

                    Text {
                        id: cestLabel
                        anchors.centerIn: parent
                        text: "CEST"
                        color: page.tzMode === "CEST" ? "#06232f" : page.colInkDim
                        font.pixelSize: 14 * page.uiScale
                        font.bold: true
                        font.letterSpacing: 1 * page.uiScale
                    }
                    MouseArea { anchors.fill: parent; onClicked: page.tzMode = "CEST" }
                }
            }

            Canvas {
                id: clock
                width: parent.width
                // Nog meer ruimte voor de wijzerplaat: geen aparte
                // "Weergave: ..."-regel meer (de knop toont dat al), dus één
                // element en één spacing-gap minder dan voorheen.
                height: parent.height - 24 * page.uiScale - toggleRow.height - stardateRow.height - 3 * (6 * page.uiScale)

                function hand(ctx, cx, cy, angle, len, width, color, tail) {
                    var x2 = cx + len * Math.sin(angle)
                    var y2 = cy - len * Math.cos(angle)
                    var x1 = cx - tail * Math.sin(angle)
                    var y1 = cy + tail * Math.cos(angle)
                    ctx.beginPath()
                    ctx.moveTo(x1, y1)
                    ctx.lineTo(x2, y2)
                    ctx.strokeStyle = color
                    ctx.lineWidth = width
                    ctx.lineCap = "round"
                    ctx.stroke()
                }

                onPaint: {
                    var ctx = getContext("2d")
                    var W = width, H = height
                    ctx.clearRect(0, 0, W, H)
                    var cx = W / 2, cy = H / 2
                    var R = Math.min(W, H) / 2 - 4 * page.uiScale

                    ctx.beginPath()
                    ctx.arc(cx, cy, R, 0, 2 * Math.PI)
                    ctx.strokeStyle = page.colIce
                    ctx.lineWidth = 2 * page.uiScale
                    ctx.stroke()

                    for (var i = 0; i < 60; i++) {
                        var a = (i / 60) * 2 * Math.PI
                        var major = (i % 5 === 0)
                        var rOuter = R - 2 * page.uiScale
                        var rInner = major ? R - 14 * page.uiScale : R - 7 * page.uiScale
                        var x1 = cx + rInner * Math.sin(a), y1 = cy - rInner * Math.cos(a)
                        var x2 = cx + rOuter * Math.sin(a), y2 = cy - rOuter * Math.cos(a)
                        ctx.beginPath()
                        ctx.moveTo(x1, y1)
                        ctx.lineTo(x2, y2)
                        var hourIdx = Math.round(i / 5) % 12
                        var quadrantGold = (hourIdx === 0 || hourIdx === 3 || hourIdx === 6 || hourIdx === 9)
                        ctx.strokeStyle = major ? (quadrantGold ? page.colGold : page.colIce) : "#4a5066"
                        ctx.lineWidth = major ? 3 * page.uiScale : 1.5 * page.uiScale
                        ctx.stroke()
                    }

                    var d = page.displayDate()
                    var h = d.getHours() % 12
                    var m = d.getMinutes()
                    var s = d.getSeconds()
                    var hAngle = ((h + m / 60) / 12) * 2 * Math.PI
                    var mAngle = ((m + s / 60) / 60) * 2 * Math.PI
                    var sAngle = (s / 60) * 2 * Math.PI

                    clock.hand(ctx, cx, cy, hAngle, R * 0.5, 5 * page.uiScale, page.colInk, R * 0.12)
                    clock.hand(ctx, cx, cy, mAngle, R * 0.72, 3.5 * page.uiScale, page.colIce, R * 0.14)
                    clock.hand(ctx, cx, cy, sAngle, R * 0.8, 1.5 * page.uiScale, page.colGold, R * 0.16)

                    ctx.beginPath()
                    ctx.arc(cx, cy, 6 * page.uiScale, 0, 2 * Math.PI)
                    ctx.fillStyle = page.colGold
                    ctx.fill()
                }

                Timer {
                    interval: 1000
                    running: true
                    repeat: true
                    onTriggered: clock.requestPaint()
                }
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
            }

            // --- stardate rechts uitgelijnd (geen "Weergave:"-regel meer) ---
            Row {
                id: stardateRow
                anchors.right: parent.right
                spacing: 6 * page.uiScale

                Text {
                    text: "STARDATE"
                    color: page.colGold
                    font.pixelSize: 11 * page.uiScale
                    font.letterSpacing: 1
                    font.bold: true
                }
                Text {
                    text: page.stardateFor(page.now).toFixed(2)
                    color: page.colInk
                    font.family: "monospace"
                    font.pixelSize: 12 * page.uiScale
                }
            }
        }
    }
}
