import QtQuick 2.15

// ChronoAnalogPage.qml — pagina 5: analoge klok (Canvas i.p.v. SVG, zelfde
// tekentechniek als de HTML-mockup: 60 tick-marks, uur/minuut/seconde-
// wijzers, gouden hub), UTC/CEST-knoppen aan weerszijden (i.p.v. een
// samen-gegroepeerde schakelaar in het midden — de knop zelf laat al zien
// welke weergave actief is, dus geen aparte "Weergave: ..."-tekst meer
// nodig), en een decoratieve "stardate"-regel rechts uitgelijnd.
//
// Sterrenbeelden-achtergrond: dezelfde echte sterrencatalogus + Alt/Az-
// berekening als SkyplotPage.qml (zelfde tijd, zelfde GPS-positie), heel
// vaag getekend ACHTER de wijzerplaat-tick-marks/wijzers — dus letterlijk
// de sterrenhemel boven de opstelplek op dit moment, niet een verzonnen
// patroon. Bewust géén losse Theme/singleton-herbruik (project vermijdt
// QML-singletons, zie projectbrief) — de astronomie-functies zijn hier
// gedupliceerd, net als bij de andere self-contained pagina's.

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

    // --- sterrenbeelden-achtergrond: eigen 30s-tijdklok (los van de
    // seconde-tikkende "now" hierboven) — zelfde reden als op de
    // skyplot-pagina: sterposities verschuiven zo langzaam dat elke
    // seconde herberekenen zinloos zwaar zou zijn.
    property date skyNow: new Date()
    Timer { interval: 30000; running: true; repeat: true; onTriggered: page.skyNow = new Date() }

    property bool hasPos: gpsdoModel.hasGpsPosition
    property real obsLat: gpsdoModel.gpsLatitude
    property real obsLon: gpsdoModel.gpsLongitude

    // Zelfde catalogus + lijnstukken als SkyplotPage.qml — zie daar voor
    // de bronvermelding van de J2000 RA/Dec/magnitude-waarden.
    property var starCatalog: ({
        dubhe:  [11.062, 61.75, 1.8],  merak: [11.031, 56.38, 2.4],
        phecda: [11.897, 53.70, 2.4],  megrez: [12.257, 57.03, 3.3],
        alioth: [12.900, 55.97, 1.8],  mizar: [13.399, 54.93, 2.2],
        alkaid: [13.792, 49.31, 1.9],
        polaris: [2.530, 89.26, 2.0],  yildun: [17.537, 86.59, 4.4],
        epsUmi: [16.767, 82.04, 4.2],  zetaUmi: [15.735, 77.79, 4.3],
        etaUmi: [16.292, 75.76, 5.0],  pherkad: [15.345, 71.83, 3.0],
        kochab: [14.845, 74.16, 2.1],
        caph: [0.153, 59.15, 2.3],  schedar: [0.675, 56.54, 2.2],
        gammaCas: [0.945, 60.72, 2.5],  ruchbah: [1.430, 60.24, 2.7],
        segin: [1.907, 63.67, 3.4],
        betelgeuse: [5.920, 7.40, 0.5],  bellatrix: [5.418, 6.35, 1.6],
        mintaka: [5.533, -0.30, 2.2],  alnilam: [5.603, -1.20, 1.7],
        alnitak: [5.680, -1.95, 1.9],  saiph: [5.795, -9.67, 2.1],
        rigel: [5.242, -8.20, 0.1],
        deneb: [20.690, 45.28, 1.3],  sadr: [20.370, 40.26, 2.2],
        albireo: [19.512, 27.96, 3.1],  gienah: [20.770, 33.97, 2.5],
        deltaCyg: [19.750, 45.13, 2.9],
        regulus: [10.140, 11.97, 1.4],  denebola: [11.818, 14.57, 2.1],
        algieba: [10.333, 19.84, 2.0],  zosma: [11.235, 20.52, 2.6],
        chertan: [11.237, 15.43, 3.3],  adhafera: [10.278, 23.42, 3.4],
        epsLeo: [9.765, 23.77, 3.0],
        antares: [16.490, -26.43, 1.1],  graffias: [16.090, -19.80, 2.6],
        dschubba: [16.005, -22.62, 2.3],  tauSco: [16.598, -28.22, 2.8],
        epsSco: [16.837, -34.29, 2.3],  muSco: [16.865, -38.04, 3.0],
        zetaSco: [16.910, -42.36, 3.6],  shaula: [17.560, -37.10, 1.6],
        acrux: [12.443, -63.10, 0.8],  gacrux: [12.520, -57.11, 1.6],
        mimosa: [12.795, -59.69, 1.3],  deltaCru: [12.252, -58.75, 2.8],
        sirius: [6.752, -16.72, -1.5],  mirzam: [6.378, -17.96, 2.0],
        wezen: [7.140, -26.39, 1.8],  adhara: [6.977, -28.97, 1.5],
        aludra: [7.402, -29.30, 2.5],
        vega: [18.615, 38.78, 0.0],  sheliak: [18.835, 33.36, 3.5],
        sulafat: [18.982, 32.69, 3.2],  zetaLyr: [18.747, 37.60, 4.3],
        delta2Lyr: [18.908, 36.90, 4.2],
        castor: [7.577, 31.89, 1.6],  pollux: [7.755, 28.03, 1.1],
        alhena: [6.628, 16.40, 1.9],  wasat: [7.335, 21.98, 3.5],
        mebsuta: [6.732, 25.13, 3.1],  tejat: [6.382, 22.51, 2.9],
        aldebaran: [4.598, 16.51, 0.9],  elnath: [5.438, 28.61, 1.7],
        zetaTau: [5.627, 21.14, 3.0],  epsTau: [4.477, 19.18, 3.5],
        arcturus: [14.262, 19.18, -0.1],  izar: [14.750, 27.07, 2.4],
        seginus: [14.535, 38.32, 3.0],  nekkar: [15.032, 40.39, 3.5],
        deltaBoo: [15.258, 33.31, 3.5]
    })

    property var constellationLines: ([
        [["dubhe","merak"],["merak","phecda"],["phecda","megrez"],["megrez","dubhe"],
         ["megrez","alioth"],["alioth","mizar"],["mizar","alkaid"]],
        [["polaris","yildun"],["yildun","epsUmi"],["epsUmi","zetaUmi"],["zetaUmi","etaUmi"],
         ["etaUmi","pherkad"],["pherkad","kochab"],["kochab","zetaUmi"]],
        [["caph","schedar"],["schedar","gammaCas"],["gammaCas","ruchbah"],["ruchbah","segin"]],
        [["betelgeuse","bellatrix"],["bellatrix","mintaka"],["mintaka","alnilam"],["alnilam","alnitak"],
         ["alnitak","betelgeuse"],["mintaka","rigel"],["alnitak","saiph"],["rigel","saiph"]],
        [["deneb","sadr"],["sadr","albireo"],["deltaCyg","sadr"],["sadr","gienah"]],
        [["epsLeo","adhafera"],["adhafera","algieba"],["algieba","regulus"],
         ["algieba","zosma"],["zosma","denebola"],["zosma","chertan"],["chertan","regulus"]],
        [["graffias","dschubba"],["dschubba","antares"],["antares","tauSco"],["tauSco","epsSco"],
         ["epsSco","muSco"],["muSco","zetaSco"],["zetaSco","shaula"]],
        [["gacrux","acrux"],["mimosa","deltaCru"]],
        [["mirzam","sirius"],["sirius","wezen"],["wezen","adhara"],["adhara","aludra"]],
        [["vega","zetaLyr"],["zetaLyr","delta2Lyr"],["delta2Lyr","sulafat"],["sulafat","sheliak"],["sheliak","zetaLyr"]],
        [["castor","pollux"],["pollux","wasat"],["wasat","alhena"],["wasat","mebsuta"],["mebsuta","tejat"]],
        [["epsTau","aldebaran"],["aldebaran","elnath"],["elnath","zetaTau"]],
        [["arcturus","izar"],["izar","nekkar"],["nekkar","seginus"],["seginus","arcturus"],["arcturus","deltaBoo"]]
    ])

    function julianDate(d) {
        var y = d.getUTCFullYear(), m = d.getUTCMonth() + 1, day = d.getUTCDate()
        var dayFrac2 = (d.getUTCHours() + d.getUTCMinutes() / 60 + d.getUTCSeconds() / 3600) / 24
        if (m <= 2) { y -= 1; m += 12 }
        var A = Math.floor(y / 100)
        var B = 2 - A + Math.floor(A / 4)
        return Math.floor(365.25 * (y + 4716)) + Math.floor(30.6001 * (m + 1)) + day + dayFrac2 + B - 1524.5
    }

    function gmstHours(jd) {
        var T = (jd - 2451545.0) / 36525.0
        var gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * T * T - (T * T * T) / 38710000.0
        gmst = gmst % 360
        if (gmst < 0) gmst += 360
        return gmst / 15.0
    }

    function altAzFor(raH, decDeg, jd) {
        var lst = page.gmstHours(jd) + page.obsLon / 15.0
        lst = ((lst % 24) + 24) % 24
        var haRad = (lst - raH) * 15.0 * Math.PI / 180
        var decRad = decDeg * Math.PI / 180
        var latRad = page.obsLat * Math.PI / 180
        var sinAlt = Math.sin(decRad) * Math.sin(latRad) + Math.cos(decRad) * Math.cos(latRad) * Math.cos(haRad)
        sinAlt = Math.max(-1, Math.min(1, sinAlt))
        var alt = Math.asin(sinAlt)
        var cosAz = (Math.sin(decRad) - Math.sin(latRad) * Math.sin(alt)) / (Math.cos(latRad) * Math.cos(alt))
        cosAz = Math.max(-1, Math.min(1, cosAz))
        var az = Math.acos(cosAz)
        if (Math.sin(haRad) > 0) az = 2 * Math.PI - az
        return { alt: alt * 180 / Math.PI, az: az * 180 / Math.PI }
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

                    // --- sterrenbeelden, vaag, ACHTER de wijzerplaat -------
                    // Zelfde polaire projectie als de skyplot (az=0/Noord
                    // boven, elevatie 90°=midden) — dus toevallig ook exact
                    // uitgelijnd met "12 uur boven" op deze klok. Functie
                    // buiten de if-block gedeclareerd (geen block-scoped
                    // function declarations — zelfde stijl als polar() op
                    // de skyplot-pagina).
                    function skyPolar(el, az) {
                        var rad = R * (1 - el / 90)
                        var a = (az - 90) * Math.PI / 180
                        return [cx + rad * Math.cos(a), cy + rad * Math.sin(a)]
                    }
                    if (page.hasPos) {
                        var jd = page.julianDate(page.skyNow)
                        var pos = {}
                        for (var name in page.starCatalog) {
                            var st = page.starCatalog[name]
                            var aa = page.altAzFor(st[0], st[1], jd)
                            if (aa.alt > 0) pos[name] = aa
                        }

                        ctx.strokeStyle = "rgba(160,175,205,0.16)"
                        ctx.lineWidth = 1 * page.uiScale
                        for (var c = 0; c < page.constellationLines.length; c++) {
                            var segs = page.constellationLines[c]
                            for (var l = 0; l < segs.length; l++) {
                                var a1 = pos[segs[l][0]], a2 = pos[segs[l][1]]
                                if (!a1 || !a2) continue
                                var p1 = skyPolar(a1.alt, a1.az), p2 = skyPolar(a2.alt, a2.az)
                                ctx.beginPath(); ctx.moveTo(p1[0], p1[1]); ctx.lineTo(p2[0], p2[1]); ctx.stroke()
                            }
                        }

                        for (var starName in pos) {
                            var mag = page.starCatalog[starName][2]
                            var sp = skyPolar(pos[starName].alt, pos[starName].az)
                            var sr = Math.max(0.6, (2.0 - mag * 0.3)) * page.uiScale
                            ctx.beginPath()
                            ctx.arc(sp[0], sp[1], sr, 0, 2 * Math.PI)
                            ctx.fillStyle = "rgba(220,228,245,0.35)"
                            ctx.fill()
                        }
                    }

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
                Connections {
                    target: page
                    function onSkyNowChanged() { clock.requestPaint() }
                    function onHasPosChanged() { clock.requestPaint() }
                    function onObsLatChanged() { clock.requestPaint() }
                    function onObsLonChanged() { clock.requestPaint() }
                }
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
