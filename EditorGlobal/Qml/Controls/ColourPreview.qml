import QtQuick 2.15
import QtQuick.Layouts 1.15

Rectangle {
    id: root

    property var rgba: [255, 255, 255, 255]

    Layout.preferredWidth: 76
    Layout.preferredHeight: 32
    clip: true

    Checkerboard {
        anchors.fill: parent
        anchors.margins: 1
        cellSize: 5
    }

    Rectangle {
        anchors.fill: parent
        anchors.margins: 1
        color: Qt.rgba(root.rgba[0] / 255, root.rgba[1] / 255, root.rgba[2] / 255, root.rgba[3] / 255)
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.color: "#373737"
    }
}
