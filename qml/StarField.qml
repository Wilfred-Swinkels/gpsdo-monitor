import QtQuick 2.15

// StarField.qml — zwak twinkelend sterrenveld op de achtergrond, zoals de
// HTML-mockup's ".starfield". Bewust een bescheiden aantal stipjes (30
// i.p.v. de 50 uit de browser-mockup) — elk stipje is een eigen
// animatie-timer, en dit draait continu op een Pi 3B+.

Item {
    id: root
    property int starCount: 30

    Repeater {
        model: root.starCount
        delegate: Rectangle {
            property real sz: 0.6 + Math.random() * 1.6
            x: Math.random() * root.width
            y: Math.random() * root.height
            width: sz
            height: sz
            radius: width / 2
            color: "#dfeefc"
            opacity: 0.15

            SequentialAnimation on opacity {
                loops: Animation.Infinite
                running: true
                PauseAnimation { duration: Math.random() * 4000 }
                NumberAnimation { to: 0.9; duration: 1800 + Math.random() * 1400; easing.type: Easing.InOutQuad }
                NumberAnimation { to: 0.15; duration: 1800 + Math.random() * 1400; easing.type: Easing.InOutQuad }
            }
        }
    }
}
