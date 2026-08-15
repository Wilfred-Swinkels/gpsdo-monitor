import QtQuick 2.15

// StatTile.qml — herbruikbare label+waarde-tegel voor de 2x3 grids.

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
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        text: tile.label
        color: tile.colInkDim
        font.pixelSize: 12
        font.letterSpacing: 1
    }

    Text {
        anchors.centerIn: parent
        text: tile.value.toString()
        color: tile.colInk
        font.pixelSize: 30
        font.bold: true
    }
}
