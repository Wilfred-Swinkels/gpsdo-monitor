import QtQuick 2.15

// OverviewPage.qml — de eerste echte pagina: lock-hero, accuracy-strip,
// lamp-veroudering-strip en een 2x3 grid met de 6 kerncijfers.
//
// Layout met expliciete x/y/width/height i.p.v. Column/GridLayout (zie
// main.qml voor de motivatie: geen impliciete-hoogte-circulariteit).
//
// De 6 grid-tegels volgen bewust exact de eerder afgesproken indeling uit
// de HTML-mockup/projectbrief (screen 1 "Overview"): lampspanning,
// xtal-spanning, sats gebruikt, SNR sats, fix-type, DAC-waarde.
//
// `uiScale` komt van main.qml (werkelijke schermbreedte / 720) — alle
// afmetingen hieronder zijn hiermee vermenigvuldigd zodat de pagina er ook
// verhoudingsgewijs goed uitziet als het scherm niet exact 720x720 blijkt
// te zijn. Zie main.qml voor de volledige uitleg.
//
// Kleuren worden van main.qml doorgegeven i.p.v. hier hardcoded, zodat er
// straks maar één plek is om het palet aan te passen.

Item {
    id: page

    property real uiScale: 1.0

    // Kleuren, doorgegeven vanuit main.qml.
    property color colBg: "#000000"
    property color colTile: "#1b1e29"
    property color colTile2: "#242838"
    property color colBorder: "#4a5066"
    property color colInk: "#c9cedd"
    property color colInkDim: "#7d8296"
    property color colGold: "#f3714f"
    property color colIce: "#80c8ec"
    // Functie state("L"/"U"/"H"/"D"/...) -> kleur, doorgegeven vanuit main.qml.
    property var stateColorFn: function (state) { return colInkDim }

    // Diepe, bijna-zwarte tint per staat voor de ::after-fade onderin de
    // lock-hero (".lock-hero::after"/"--hero-fade-rgb" in de HTML-mockup) —
    // NIET dezelfde kleur als stateColorFn (die is de volle, felle tint voor
    // de achtergrondvulling); dit is bewust extra donker gemaakt zodat de
    // witte shine-sweep er goed op afsteekt. Alleen hier nodig, dus lokaal
    // i.p.v. ook nog een prop erbij op main.qml.
    function stateFadeColor(state) {
        switch (state) {
        case "L": return Qt.rgba(1 / 255, 10 / 255, 7 / 255, 1)
        case "H": return Qt.rgba(42 / 255, 15 / 255, 4 / 255, 1)
        case "U": return Qt.rgba(42 / 255, 39 / 255, 8 / 255, 1)
        case "D": return Qt.rgba(42 / 255, 7 / 255, 5 / 255, 1)
        default:  return Qt.rgba(0, 0, 0, 1)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"

        // --- Lock hero — alleen state, geen DAC-waarde meer hier -----------
        // DAC-waarde staat (zoals afgesproken) in de 6-tegel-grid hieronder,
        // niet ook nog los in de hero — dat was dubbelop.
        Rectangle {
            id: lockHero
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10 * page.uiScale
            height: 118 * page.uiScale
            radius: 8 * page.uiScale
            // Volle, felle staatskleur als achtergrondvulling — dit IS de
            // "groen gevuld vlak" uit de mockup (".lock-hero{background:
            // var(--blue)}"), geen rand — de eerdere versie had dit per
            // ongeluk omgedraaid (donkere tegel + gekleurde rand, die rand
            // heb ik zelf verzonnen, staat niet in de mockup).
            color: page.stateColorFn(gpsdoModel.lockState)
            clip: true

            // --- "Star Trek" helderheids-fade + shine-sweep (".lock-hero::after"
            // en ".shine" uit de HTML-mockup) — decoratief, onder de tekst.
            // De fade is een donkere tint van de staatskleur, van ondoorzichtig
            // onderin naar transparant op de bovenste helft — geeft de vulling
            // diepte i.p.v. een vlak blok.
            Rectangle {
                id: heroFade
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0.0;  color: "transparent" }
                    GradientStop { position: 0.5;  color: "transparent" }
                    GradientStop { position: 0.66; color: Qt.rgba(page.stateFadeColor(gpsdoModel.lockState).r, page.stateFadeColor(gpsdoModel.lockState).g, page.stateFadeColor(gpsdoModel.lockState).b, 0.2) }
                    GradientStop { position: 0.82; color: Qt.rgba(page.stateFadeColor(gpsdoModel.lockState).r, page.stateFadeColor(gpsdoModel.lockState).g, page.stateFadeColor(gpsdoModel.lockState).b, 0.45) }
                    GradientStop { position: 1.0;  color: Qt.rgba(page.stateFadeColor(gpsdoModel.lockState).r, page.stateFadeColor(gpsdoModel.lockState).g, page.stateFadeColor(gpsdoModel.lockState).b, 0.65) }
                }
            }

            Item {
                id: shineMask
                anchors.fill: parent
                clip: true

                Rectangle {
                    id: shine
                    width: lockHero.width * 0.3
                    height: lockHero.height * 2.4
                    y: -lockHero.height * 0.7
                    x: -width
                    rotation: 18
                    transformOrigin: Item.Center
                    gradient: Gradient {
                        orientation: Gradient.Horizontal
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 0.5; color: Qt.rgba(1, 1, 1, 0.42) }
                        GradientStop { position: 1.0; color: "transparent" }
                    }

                    SequentialAnimation on x {
                        loops: Animation.Infinite
                        NumberAnimation { from: -shine.width; to: lockHero.width + shine.width; duration: 4500; easing.type: Easing.InOutQuad }
                        PauseAnimation { duration: 2500 }
                    }
                }
            }

            Row {
                anchors.left: parent.left
                anchors.leftMargin: 20 * page.uiScale
                anchors.right: parent.right
                anchors.rightMargin: 20 * page.uiScale
                anchors.verticalCenter: parent.verticalCenter
                spacing: 16 * page.uiScale

                Text {
                    // Groot glyph-teken (".glyph" — hetzelfde staatsteken als op
                    // de statustegels op de FLL-state-pagina).
                    text: gpsdoModel.lockState
                    color: "#ffffff"
                    font.pixelSize: 58 * page.uiScale
                    font.bold: true
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - 58 * page.uiScale - 16 * page.uiScale
                    spacing: 3 * page.uiScale

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: gpsdoModel.lockLabel
                        color: "#ffffff"
                        font.pixelSize: 21 * page.uiScale
                        font.bold: true
                        font.letterSpacing: 1 * page.uiScale
                    }

                    Text {
                        width: parent.width
                        elide: Text.ElideRight
                        text: gpsdoModel.lockSubText
                        color: "#ffffff"
                        opacity: 0.85
                        font.pixelSize: 11 * page.uiScale
                        font.family: "monospace"
                    }
                }
            }
        }

        // --- Accuracy-strip (Δf/f) — dubbele hoogte, grotere tekst ----------
        Rectangle {
            id: accStrip1
            anchors.top: lockHero.bottom
            anchors.topMargin: 8 * page.uiScale
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10 * page.uiScale
            height: 88 * page.uiScale
            radius: 6 * page.uiScale
            color: page.colTile2

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16 * page.uiScale
                anchors.top: parent.top
                anchors.topMargin: 12 * page.uiScale
                text: "Δf/f"
                color: page.colInkDim
                font.pixelSize: 17 * page.uiScale
            }
            // Op verzoek: de instant Δf/f-waarde (was hier eerst, colGold/
            // 28px) is vervangen door het langzaam verfijnende gemiddelde
            // hieronder — zelfde tegel, zelfde kleur/tekstgrootte als de
            // instant-waarde had. Zie GpsdoModel::accuracyAvgText() voor het
            // waarom (middelt de 6,25e-9-kwantisatie van een losse sample
            // geleidelijk weg). Software-only, raakt de FLL-mode niet aan.
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16 * page.uiScale
                anchors.left: parent.left
                anchors.leftMargin: 90 * page.uiScale
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideLeft
                anchors.top: parent.top
                anchors.topMargin: 6 * page.uiScale
                text: gpsdoModel.accuracyAvgText
                color: page.colGold
                font.family: "monospace"
                font.pixelSize: 28 * page.uiScale
                font.bold: true
            }
        }

        // --- Lampspanning-veroudering-strip ---------------------------------
        // Per ontwerp: laatste 1x/uur-sample + trend (V/dag), losstaand van
        // de live lampspanning die ook in de grid hieronder staat. Sparkline/
        // trendcijfer/ringbuffer zijn nog niet gebouwd (staat in Volgende
        // stap) — voorlopig alleen de placeholder-tekst.
        Rectangle {
            id: accStrip2
            anchors.top: accStrip1.bottom
            anchors.topMargin: 6 * page.uiScale
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.margins: 10 * page.uiScale
            height: 88 * page.uiScale
            radius: 6 * page.uiScale
            color: page.colTile2

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 16 * page.uiScale
                anchors.right: parent.right
                anchors.rightMargin: 90 * page.uiScale
                anchors.verticalCenter: parent.verticalCenter
                elide: Text.ElideRight
                text: "Lampspanning — veroudering"
                color: page.colInkDim
                font.pixelSize: 16 * page.uiScale
            }
            Text {
                anchors.right: parent.right
                anchors.rightMargin: 16 * page.uiScale
                anchors.verticalCenter: parent.verticalCenter
                text: "—"
                color: page.colInkDim
                font.family: "monospace"
                font.pixelSize: 26 * page.uiScale
            }
        }

        // --- 2x3 grid: lampspanning, xtal-spanning, sats gebruikt, --------
        //     SNR sats, fix-type, DAC-waarde (vaste volgorde, zie boven) ---
        Item {
            id: grid
            anchors.top: accStrip2.bottom
            anchors.topMargin: 8 * page.uiScale
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10 * page.uiScale

            property real tileSpacing: 8 * page.uiScale
            property real tileWidth: (width - tileSpacing) / 2
            property real tileHeight: (height - 2 * tileSpacing) / 3

            StatTile {
                x: 0
                y: 0
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "LAMP"
                value: gpsdoModel.lampVoltageText
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: 0
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "XTAL"
                value: gpsdoModel.xtalVoltageText
            }
            StatTile {
                x: 0
                y: grid.tileHeight + grid.tileSpacing
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "SATS GEBRUIKT"
                value: gpsdoModel.hasGpsData ? gpsdoModel.satsUsed : "—"
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: grid.tileHeight + grid.tileSpacing
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "SNR SATS"
                value: gpsdoModel.snrAvgText
            }
            StatTile {
                x: 0
                y: 2 * (grid.tileHeight + grid.tileSpacing)
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "FIX TYPE"
                value: gpsdoModel.fixTypeText
            }
            StatTile {
                x: grid.tileWidth + grid.tileSpacing
                y: 2 * (grid.tileHeight + grid.tileSpacing)
                width: grid.tileWidth
                height: grid.tileHeight
                uiScale: page.uiScale
                colTile: page.colTile
                colBorder: page.colBorder
                colInkDim: page.colInkDim
                colInk: page.colInk
                label: "DAC"
                value: gpsdoModel.dacHex
            }
        }
    }
}
