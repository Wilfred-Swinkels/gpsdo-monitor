import QtQuick 2.15

// StatTile.qml — herbruikbare label+waarde-tegel voor de 2x3 grids.
//
// Label vast bovenaan, waarde vast onderaan (allebei met elide) — bewust
// NIET de waarde gecentreerd over de hele tegel, want dat overlapte in de
// praktijk met een lang label (bv. "SATS ZICHTBAAR" + "16") zodra het label
// tot voorbij het midden van de tegel liep. Met vaste top/bottom-posities
// kan dat niet meer gebeuren, ongeacht labellengte of tegelgrootte.

Rectangle {
    id: tile

    property string label: ""
    property var value: "—"

    property color colTile: "#1b1e29"
    property color colBorder: "#4a5066"
    property color colInkDim: "#7d8296"
    property color colInk: "#c9cedd"

    radius: 6
    color: colTile
    border.width: 1
    border.color: colBorder

    Text {
        id: labelText
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 10
        text: tile.label
        color: tile.colInkDim
        font.pixelSize: 12
        font.letterSpacing: 1
        elide: Text.ElideRight
    }

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        horizontalAlignment: Text.AlignHCenter
        text: tile.value.toString()
        color: tile.colInk
        font.pixelSize: 26
        font.bold: true
        elide: Text.ElideRight
    }
}
