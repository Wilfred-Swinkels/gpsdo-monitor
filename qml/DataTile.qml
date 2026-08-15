import QtQuick 2.15

// DataTile.qml — herbruikbare tegel voor de FLL-state/GPS-fix/nauwkeurig-
// heid-pagina's: waarde BOVEN, label ONDER (".data-tile .v"/".k" in de
// HTML-mockup — andere volgorde dan StatTile op Overview, met opzet: dit
// volgt de mockup exact voor deze nieuwe pagina's).

Rectangle {
    id: tile

    property string label: ""
    property var value: "—"
    property real uiScale: 1.0
    property int valuePixelSize: 20

    property color colTile: "#1b1e29"
    property color colBorder: "#4a5066"
    property color colInkDim: "#7d8296"
    property color colIce: "#80c8ec"

    radius: 14 * uiScale
    color: colTile
    border.width: 1
    border.color: colBorder
    clip: true

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: 12 * tile.uiScale
        spacing: 3 * tile.uiScale

        Text {
            width: parent.width
            elide: Text.ElideRight
            text: tile.value.toString()
            color: tile.colIce
            font.pixelSize: tile.valuePixelSize * tile.uiScale
            font.bold: true
        }
        Text {
            width: parent.width
            elide: Text.ElideRight
            text: tile.label
            color: tile.colInkDim
            font.pixelSize: 10 * tile.uiScale
            font.letterSpacing: 1
        }
    }
}
