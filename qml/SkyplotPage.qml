import QtQuick 2.15

// SkyplotPage.qml — pagina 2: polair diagram van alle zichtbare satellieten
// (UBX NAV-SVINFO via GpsdoModel.satellites), gekleurd naar CN0
// (signaalsterkte). Canvas-based, zelfde polaire projectie als de SVG-versie
// in de HTML-mockup (elevatie 90°=midden, 0°=rand; noord boven).
//
// Sterrenbeelden-overlay ("ster, draadje, ster"): een kleine, echte
// sterrencatalogus (J2000 RA/Dec, uit vaste-sterren-referentietabellen) die
// via de klassieke Local-Sidereal-Time-formule naar Alt/Az wordt omgerekend
// voor de actuele UTC-tijd + de echte GPS-positie (NAV-POSLLH via
// GpsdoModel.gpsLatitude/gpsLongitude — vereist dus een fix met positie,
// niet zomaar verzonnen). Precessie/nutatie/refractie/eigenbeweging worden
// bewust genegeerd (verwaarloosbaar op deze schaal/nauwkeurigheid) — dit is
// een decoratieve maar astronomisch correcte plaatsing, geen navigatie-tool.
// Sterren/lijnen onder de horizon (alt<=0) worden niet getekend.

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

    // Op verzoek omgezet van de oorspronkelijke blauwe schaal naar geel —
    // zelfde 5-staps opbouw (donker=zwak -> fel=sterk), alleen de kleurfamilie
    // is anders. Bovenste stop is bewust dezelfde tint (#ffe600) als de
    // felgele "actief in fix"-markering hierboven: een sterk EN gebruikt
    // signaal mag er ook als zodanig uitzien.
    function cn0Color(cn0) {
        var stops = ["#221b02", "#4a3c05", "#7a640a", "#c9a812", "#ffe600"]
        var idx = Math.min(stops.length - 1, Math.floor((cn0 / 50) * stops.length))
        return stops[Math.max(0, idx)]
    }

    // --- live tijd (voor de sterposities) + GPS-positie ---------------------
    property date now: new Date()
    Timer { interval: 30000; running: true; repeat: true; onTriggered: page.now = new Date() }

    property bool hasPos: gpsdoModel.hasGpsPosition
    property real obsLat: gpsdoModel.gpsLatitude
    property real obsLon: gpsdoModel.gpsLongitude

    // --- sterrencatalogus: naam -> [RA in uren, Dec in graden, magnitude] --
    // J2000-coördinaten van de heldere sterren die de bekende figuren vormen.
    property var starCatalog: ({
        // Grote Beer (Ursa Major)
        dubhe:  [11.062, 61.75, 1.8],  merak: [11.031, 56.38, 2.4],
        phecda: [11.897, 53.70, 2.4],  megrez: [12.257, 57.03, 3.3],
        alioth: [12.900, 55.97, 1.8],  mizar: [13.399, 54.93, 2.2],
        alkaid: [13.792, 49.31, 1.9],
        // Kleine Beer (Ursa Minor)
        polaris: [2.530, 89.26, 2.0],  yildun: [17.537, 86.59, 4.4],
        epsUmi: [16.767, 82.04, 4.2],  zetaUmi: [15.735, 77.79, 4.3],
        etaUmi: [16.292, 75.76, 5.0],  pherkad: [15.345, 71.83, 3.0],
        kochab: [14.845, 74.16, 2.1],
        // Cassiopeia
        caph: [0.153, 59.15, 2.3],  schedar: [0.675, 56.54, 2.2],
        gammaCas: [0.945, 60.72, 2.5],  ruchbah: [1.430, 60.24, 2.7],
        segin: [1.907, 63.67, 3.4],
        // Orion
        betelgeuse: [5.920, 7.40, 0.5],  bellatrix: [5.418, 6.35, 1.6],
        mintaka: [5.533, -0.30, 2.2],  alnilam: [5.603, -1.20, 1.7],
        alnitak: [5.680, -1.95, 1.9],  saiph: [5.795, -9.67, 2.1],
        rigel: [5.242, -8.20, 0.1],
        // Zwaan (Cygnus, Noordelijk Kruis)
        deneb: [20.690, 45.28, 1.3],  sadr: [20.370, 40.26, 2.2],
        albireo: [19.512, 27.96, 3.1],  gienah: [20.770, 33.97, 2.5],
        deltaCyg: [19.750, 45.13, 2.9],
        // Leeuw (Leo)
        regulus: [10.140, 11.97, 1.4],  denebola: [11.818, 14.57, 2.1],
        algieba: [10.333, 19.84, 2.0],  zosma: [11.235, 20.52, 2.6],
        chertan: [11.237, 15.43, 3.3],  adhafera: [10.278, 23.42, 3.4],
        epsLeo: [9.765, 23.77, 3.0],
        // Schorpioen (Scorpius)
        antares: [16.490, -26.43, 1.1],  graffias: [16.090, -19.80, 2.6],
        dschubba: [16.005, -22.62, 2.3],  tauSco: [16.598, -28.22, 2.8],
        epsSco: [16.837, -34.29, 2.3],  muSco: [16.865, -38.04, 3.0],
        zetaSco: [16.910, -42.36, 3.6],  shaula: [17.560, -37.10, 1.6],
        // Zuiderkruis (Crux)
        acrux: [12.443, -63.10, 0.8],  gacrux: [12.520, -57.11, 1.6],
        mimosa: [12.795, -59.69, 1.3],  deltaCru: [12.252, -58.75, 2.8],
        // Grote Hond (Canis Major)
        sirius: [6.752, -16.72, -1.5],  mirzam: [6.378, -17.96, 2.0],
        wezen: [7.140, -26.39, 1.8],  adhara: [6.977, -28.97, 1.5],
        aludra: [7.402, -29.30, 2.5],
        // Lier (Lyra)
        vega: [18.615, 38.78, 0.0],  sheliak: [18.835, 33.36, 3.5],
        sulafat: [18.982, 32.69, 3.2],  zetaLyr: [18.747, 37.60, 4.3],
        delta2Lyr: [18.908, 36.90, 4.2],
        // Tweelingen (Gemini)
        castor: [7.577, 31.89, 1.6],  pollux: [7.755, 28.03, 1.1],
        alhena: [6.628, 16.40, 1.9],  wasat: [7.335, 21.98, 3.5],
        mebsuta: [6.732, 25.13, 3.1],  tejat: [6.382, 22.51, 2.9],
        // Stier (Taurus, gedeeltelijk)
        aldebaran: [4.598, 16.51, 0.9],  elnath: [5.438, 28.61, 1.7],
        zetaTau: [5.627, 21.14, 3.0],  epsTau: [4.477, 19.18, 3.5],
        // Ossenhoeder (Boötes, gedeeltelijk)
        arcturus: [14.262, 19.18, -0.1],  izar: [14.750, 27.07, 2.4],
        seginus: [14.535, 38.32, 3.0],  nekkar: [15.032, 40.39, 3.5],
        deltaBoo: [15.258, 33.31, 3.5]
    })

    // Elk element = 1 sterrenbeeld: een reeks lijnstukken (paren van
    // catalogus-namen) die samen de vertrouwde stok-figuur vormen.
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

    // --- astronomie: Juliaanse dag -> Greenwich sterrentijd -> Alt/Az -------
    function julianDate(d) {
        var y = d.getUTCFullYear(), m = d.getUTCMonth() + 1, day = d.getUTCDate()
        var dayFrac = (d.getUTCHours() + d.getUTCMinutes() / 60 + d.getUTCSeconds() / 3600) / 24
        if (m <= 2) { y -= 1; m += 12 }
        var A = Math.floor(y / 100)
        var B = 2 - A + Math.floor(A / 4)
        return Math.floor(365.25 * (y + 4716)) + Math.floor(30.6001 * (m + 1)) + day + dayFrac + B - 1524.5
    }

    function gmstHours(jd) {
        var T = (jd - 2451545.0) / 36525.0
        var gmst = 280.46061837 + 360.98564736629 * (jd - 2451545.0) + 0.000387933 * T * T - (T * T * T) / 38710000.0
        gmst = gmst % 360
        if (gmst < 0) gmst += 360
        return gmst / 15.0
    }

    // Retourneert {alt, az} in graden, of null als er geen positie is.
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

    Rectangle {
        anchors.fill: parent
        color: "transparent"

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

                    ctx.strokeStyle = page.colBorder
                    ctx.lineWidth = 1.5 * page.uiScale
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

                    // --- sterrenbeelden: "ster, draadje, ster" --------------
                    // Alleen tekenen zodra we een echte positie hebben (NAV-
                    // POSLLH) — zonder waarnemerspositie kan Alt/Az niet
                    // kloppend berekend worden, en dan liever niets tonen dan
                    // iets verzonnens.
                    if (page.hasPos) {
                        var jd = page.julianDate(page.now)
                        var pos = {}
                        for (var name in page.starCatalog) {
                            var s = page.starCatalog[name]
                            var aa = page.altAzFor(s[0], s[1], jd)
                            if (aa.alt > 0) pos[name] = aa
                        }

                        ctx.strokeStyle = "rgba(210,220,255,0.55)"
                        ctx.lineWidth = 1.3 * page.uiScale
                        for (var c = 0; c < page.constellationLines.length; c++) {
                            var segs = page.constellationLines[c]
                            for (var l = 0; l < segs.length; l++) {
                                var a1 = pos[segs[l][0]], a2 = pos[segs[l][1]]
                                if (!a1 || !a2) continue
                                var p1 = polar(a1.alt, a1.az), p2 = polar(a2.alt, a2.az)
                                ctx.beginPath(); ctx.moveTo(p1[0], p1[1]); ctx.lineTo(p2[0], p2[1]); ctx.stroke()
                            }
                        }

                        for (var starName in pos) {
                            var mag = page.starCatalog[starName][2]
                            var sp = polar(pos[starName].alt, pos[starName].az)
                            var sr = Math.max(1.1, (3.0 - mag * 0.35)) * page.uiScale
                            ctx.beginPath()
                            ctx.arc(sp[0], sp[1], sr, 0, 2 * Math.PI)
                            ctx.fillStyle = "rgba(255,255,255,0.98)"
                            ctx.fill()
                            // Kleine gloed omheen — maakt de heldere sterren
                            // beter herkenbaar tussen de satellietstippen.
                            ctx.beginPath()
                            ctx.arc(sp[0], sp[1], sr * 2.2, 0, 2 * Math.PI)
                            ctx.fillStyle = "rgba(210,225,255,0.18)"
                            ctx.fill()
                        }
                    }

                    var sats = sky.sats || []
                    for (var s2 = 0; s2 < sats.length; s2++) {
                        var sat = sats[s2]
                        var satPos = polar(sat.elevationDeg, sat.azimuthDeg)
                        var r = (7 + (sat.cno / 50) * 6) * page.uiScale
                        ctx.beginPath()
                        ctx.arc(satPos[0], satPos[1], r, 0, 2 * Math.PI)
                        // Actief in de fix (usedInFix) -> fel geel, in plaats
                        // van de normale CN0-signaalsterkte-kleur — zo is in
                        // één oogopslag te zien welke sats daadwerkelijk
                        // meetellen, los van hoe sterk hun signaal is.
                        ctx.fillStyle = sat.usedInFix ? "#ffe600" : page.cn0Color(sat.cno)
                        ctx.fill()
                        ctx.strokeStyle = "#000000"
                        ctx.lineWidth = 1.5
                        ctx.stroke()

                        ctx.fillStyle = "#ffffff"
                        ctx.strokeStyle = "#000000"
                        ctx.lineWidth = 2
                        ctx.font = "bold " + (9 * page.uiScale) + "px sans-serif"
                        ctx.textAlign = "center"
                        ctx.strokeText(String(sat.svid), satPos[0], satPos[1] + 3 * page.uiScale)
                        ctx.fillText(String(sat.svid), satPos[0], satPos[1] + 3 * page.uiScale)
                    }
                }

                onSatsChanged: requestPaint()
                onWidthChanged: requestPaint()
                onHeightChanged: requestPaint()
                Connections {
                    target: page
                    function onNowChanged() { sky.requestPaint() }
                    function onHasPosChanged() { sky.requestPaint() }
                    function onObsLatChanged() { sky.requestPaint() }
                    function onObsLonChanged() { sky.requestPaint() }
                }
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
                        GradientStop { position: 0.0; color: "#221b02" }
                        GradientStop { position: 0.3; color: "#4a3c05" }
                        GradientStop { position: 0.55; color: "#7a640a" }
                        GradientStop { position: 0.8; color: "#c9a812" }
                        GradientStop { position: 1.0; color: "#ffe600" }
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
