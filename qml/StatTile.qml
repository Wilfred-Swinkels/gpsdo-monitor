import QtQuick 2.15

// StatTile.qml — herbruikbare label+waarde-tegel voor de 2x3 grids.
//
// De waarde is bewust verankerd ONDER het label (anchors.top: labelText.bottom)
// in plaats van los aan de onderkant van de tegel — met twee onafhankelijk
// verankerde teksten (label boven, waarde onder) bleek op de echte hardware
// dat ze alsnog overlapten zodra de tegel kleiner was dan verwacht (zie
// projectbrief/chatgeschiedenis: foto liet "SATS GEBRUIK9" en dergelijke
// zien). Met deze keten kán dat niet meer: de waarde begint pas ná het
// label, hoe klein de tegel ook is. `clip: true` op de tegel zelf voorkomt
// dat de waarde zichtbaar buiten de tegelrand loopt als de tegel écht te
// klein is.
//
// `uiScale` komt via OverviewPage door vanuit main.qml (werkelijke
// schermbreedte / 720) — zie main.qml voor de volledige uitleg.

Rectangle {
    id: tile

    property string label: ""
    property var value: "—"
    property real uiScale: 1.0

    property color colTile: "#1b1e29"
    property color colBorder: "#4a5066"
    property color colInkDim: "#7d8296"
    property color colInk: "#c9cedd"

    radius: 6 * uiScale
    color: colTile
    border.width: 1
    border.color: colBorder
    clip: true

    Text {
        id: labelText
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8 * tile.uiScale
        text: tile.label
        color: tile.colInkDim
        font.pixelSize: 11 * tile.uiScale
        font.letterSpacing: 1
        elide: Text.ElideRight
    }

    Text {
        // Vult de hele ruimte ónder het label (niet alleen de eigen
        // tekst-hoogte) en centreert de waarde daarbinnen, zowel
        // horizontaal als verticaal — vandaar zowel top ALS bottom
        // verankerd. De keten (top: labelText.bottom) blijft staan, dus dit
        // kan nog steeds nooit over het label heen lopen.
        anchors.top: labelText.bottom
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 6 * tile.uiScale
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        text: tile.value.toString()
        color: tile.colInk
        font.pixelSize: 30 * tile.uiScale
        font.bold: true
        elide: Text.ElideRight
    }
}
