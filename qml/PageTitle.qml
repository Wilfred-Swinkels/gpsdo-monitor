import QtQuick 2.15

// PageTitle.qml — herbruikbare paginatitel + gradiënt-onderstreping, zoals
// ".page-title" in de HTML-mockup (op elke pagina hetzelfde).

Item {
    id: root
    property string text: ""
    property real uiScale: 1.0
    property color colIce: "#80c8ec"
    property color colGold: "#f3714f"

    height: 24 * uiScale

    Text {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 6 * root.uiScale
        elide: Text.ElideRight
        text: root.text.toUpperCase()
        color: root.colIce
        font.pixelSize: 13 * root.uiScale
        font.bold: true
        font.letterSpacing: 2 * root.uiScale
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 2 * root.uiScale
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: root.colGold }
            GradientStop { position: 0.4; color: root.colIce }
            GradientStop { position: 1.0; color: "transparent" }
        }
    }
}
