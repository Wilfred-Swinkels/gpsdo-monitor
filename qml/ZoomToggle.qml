import QtQuick 2.15

// ZoomToggle.qml — herbruikbare rij pil-knoppen voor tijdvenster-keuze
// (".tz-opt" in de HTML-mockup — daar ook gebruikt voor de UTC/CEST-
// schakelaar op de analoge chronometer-pagina).

Row {
    id: root
    property var options: []       // [{label:"30m", value:30}, ...]
    property var selectedValue: null
    property real uiScale: 1.0
    property color colTile: "#1b1e29"
    property color colBorder: "#4a5066"
    property color colIce: "#80c8ec"
    property color colInkDim: "#7d8296"
    signal selected(var value)

    spacing: 6 * uiScale
    height: 28 * uiScale

    Repeater {
        model: root.options
        delegate: Rectangle {
            height: root.height
            width: label.implicitWidth + 22 * root.uiScale
            radius: height / 2
            color: modelData.value === root.selectedValue ? root.colIce : root.colTile
            border.width: 2 * root.uiScale
            border.color: modelData.value === root.selectedValue ? root.colIce : root.colBorder

            Text {
                id: label
                anchors.centerIn: parent
                text: modelData.label
                color: modelData.value === root.selectedValue ? "#06232f" : root.colInkDim
                font.pixelSize: 11 * root.uiScale
                font.bold: true
                font.letterSpacing: 1 * root.uiScale
            }

            MouseArea {
                anchors.fill: parent
                onClicked: root.selected(modelData.value)
            }
        }
    }
}
