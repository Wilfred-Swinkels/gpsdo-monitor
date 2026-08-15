import QtQuick 2.15

// ChronoDigitalPage.qml — pagina 6: twee even grote digi-groups naast
// elkaar: links lokale tijd (CEST, goud), rechts UTC (ijsblauw), elk met
// H/M/S in omrande blokken en knipperende ':'-scheidingstekens, plus een
// gedeelde datumregel per kant en een gedeelde stardate-regel onderaan.

Item {
    id: page

    property real uiScale: 1.0
    property color colBg: "#000000"
    property color colTile: "#1b1e29"
    property color colSurface: "#0d0f14"
    property color colBorder: "#4a5066"
    property color colInk: "#c9cedd"
    property color colInkDim: "#7d8296"
    property color colGold: "#f3714f"
    property color colIce: "#80c8ec"

    property date now: new Date()
    property bool blinkOn: true

    Timer { interval: 1000; running: true; repeat: true; onTriggered: page.now = new Date() }
    Timer { interval: 500; running: true; repeat: true; onTriggered: page.blinkOn = !page.blinkOn }

    function pad2(n) { return (n < 10 ? "0" : "") + n }

    function localDate() {
        var d = new Date(page.now.getTime())
        d.setHours(d.getHours() + 2)
        return d
    }

    function stardateFor(d) {
        var dayStart = new Date(d.getFullYear(), d.getMonth(), d.getDate())
        var dayFrac = (d.getTime() - dayStart.getTime()) / 86400000
        return 40000 + dayFrac * 30
    }

    property var dayNames: ["zo", "ma", "di", "wo", "do", "vr", "za"]

    function dateLine(d) {
        return page.dayNames[d.getDay()] + " " + pad2(d.getDate()) + "-" + pad2(d.getMonth() + 1) + "-" + d.getFullYear()
    }

    Rectangle {
        anchors.fill: parent
        color: page.colBg

        Column {
            anchors.fill: parent
            anchors.margins: 14 * page.uiScale
            spacing: 10 * page.uiScale

            PageTitle {
                width: parent.width
                uiScale: page.uiScale
                colIce: page.colIce
                colGold: page.colGold
                text: "Chronometer — digitaal"
            }

            Row {
                id: groupsRow
                width: parent.width
                height: parent.height - 24 * page.uiScale - stardateFooter.height - 2 * (10 * page.uiScale)
                spacing: 10 * page.uiScale

                // --- lokale tijd (CEST) ---------------------------------
                Rectangle {
                    width: (groupsRow.width - groupsRow.spacing) / 2
                    height: groupsRow.height
                    radius: 14 * page.uiScale
                    color: page.colSurface
                    border.width: 1
                    border.color: page.colBorder

                    Column {
                        anchors.centerIn: parent
                        spacing: 8 * page.uiScale

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "LOKAAL — CEST"
                            color: page.colGold
                            font.pixelSize: 12 * page.uiScale
                            font.letterSpacing: 1
                            font.bold: true
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4 * page.uiScale

                            Repeater {
                                model: [ page.pad2(page.localDate().getHours()),
                                         page.pad2(page.localDate().getMinutes()),
                                         page.pad2(page.localDate().getSeconds()) ]
                                delegate: Row {
                                    spacing: 4 * page.uiScale
                                    Rectangle {
                                        width: 44 * page.uiScale
                                        height: 44 * page.uiScale
                                        radius: 8 * page.uiScale
                                        color: page.colTile
                                        border.width: 1
                                        border.color: page.colGold
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: page.colGold
                                            font.family: "monospace"
                                            font.bold: true
                                            font.pixelSize: 22 * page.uiScale
                                        }
                                    }
                                    Text {
                                        visible: index < 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: ":"
                                        color: page.colGold
                                        opacity: page.blinkOn ? 1.0 : 0.15
                                        font.bold: true
                                        font.pixelSize: 26 * page.uiScale
                                    }
                                }
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: page.dateLine(page.localDate())
                            color: page.colInkDim
                            font.pixelSize: 12 * page.uiScale
                            font.family: "monospace"
                        }
                    }
                }

                // --- UTC --------------------------------------------------
                Rectangle {
                    width: (groupsRow.width - groupsRow.spacing) / 2
                    height: groupsRow.height
                    radius: 14 * page.uiScale
                    color: page.colSurface
                    border.width: 1
                    border.color: page.colBorder

                    Column {
                        anchors.centerIn: parent
                        spacing: 8 * page.uiScale

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "UTC"
                            color: page.colIce
                            font.pixelSize: 12 * page.uiScale
                            font.letterSpacing: 1
                            font.bold: true
                        }

                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 4 * page.uiScale

                            Repeater {
                                model: [ page.pad2(page.now.getHours()),
                                         page.pad2(page.now.getMinutes()),
                                         page.pad2(page.now.getSeconds()) ]
                                delegate: Row {
                                    spacing: 4 * page.uiScale
                                    Rectangle {
                                        width: 44 * page.uiScale
                                        height: 44 * page.uiScale
                                        radius: 8 * page.uiScale
                                        color: page.colTile
                                        border.width: 1
                                        border.color: page.colIce
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData
                                            color: page.colIce
                                            font.family: "monospace"
                                            font.bold: true
                                            font.pixelSize: 22 * page.uiScale
                                        }
                                    }
                                    Text {
                                        visible: index < 2
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: ":"
                                        color: page.colIce
                                        opacity: page.blinkOn ? 1.0 : 0.15
                                        font.bold: true
                                        font.pixelSize: 26 * page.uiScale
                                    }
                                }
                            }
                        }

                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: page.dateLine(page.now)
                            color: page.colInkDim
                            font.pixelSize: 12 * page.uiScale
                            font.family: "monospace"
                        }
                    }
                }
            }

            Row {
                id: stardateFooter
                anchors.horizontalCenter: parent.horizontalCenter
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
