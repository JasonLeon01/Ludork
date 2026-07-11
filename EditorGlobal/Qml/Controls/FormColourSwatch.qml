import QtQuick 2.15
import QtQuick.Controls 2.15

Item {
    id: root

    property int r: 255
    property int g: 255
    property int b: 255
    property int a: 255

    signal clicked()

    implicitWidth: 60
    implicitHeight: 34

    Rectangle {
        anchors.fill: parent
        color: mouseArea.pressed ? dialogTheme.focusOverlayColor : dialogTheme.surfaceColor
        border.color: mouseArea.containsMouse ? dialogTheme.accentColor : dialogTheme.inputBorderColor
        border.width: 1
        radius: 3
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 5
        clip: true

        Rectangle {
            anchors.fill: parent
            color: "#cccccc"
        }

        Checkerboard {
            anchors.fill: parent
            cellSize: 5
            lightColor: "#cccccc"
            darkColor: "#ffffff"
        }

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(
                Math.round(root.r) / 255,
                Math.round(root.g) / 255,
                Math.round(root.b) / 255,
                Math.round(root.a) / 255
            )
        }

        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.width: 1
            border.color: "#323232"
        }
    }

    MouseArea {
        id: mouseArea

        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        hoverEnabled: true
        onClicked: root.clicked()
    }
}
