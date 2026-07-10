import QtQuick 2.15

Item {
    id: control

    property string label: ""
    property string fontFamily: ""
    signal clicked()

    implicitWidth: 432
    implicitHeight: 48
    activeFocusOnTab: true

    Rectangle {
        anchors.fill: parent
        color: mouseArea.pressed
            ? Qt.rgba(1, 1, 1, 200 / 255)
            : mouseArea.containsMouse ? Qt.rgba(1, 1, 1, 150 / 255) : "#1f1f1f"
        border.width: mouseArea.containsMouse || control.activeFocus ? 2 : 1
        border.color: mouseArea.containsMouse || control.activeFocus
            ? Qt.rgba(1, 1, 1, 220 / 255)
            : "#3a3a3a"
        radius: 6
    }

    Text {
        anchors.centerIn: parent
        anchors.verticalCenterOffset: mouseArea.pressed ? 2 : 0
        text: control.label
        color: mouseArea.containsMouse ? "#111111" : "#ffffff"
        font.family: control.fontFamily
        font.pixelSize: 18
        renderType: Text.NativeRendering
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onPressed: control.forceActiveFocus()
        onClicked: control.clicked()
    }

    Keys.onReturnPressed: control.clicked()
    Keys.onEnterPressed: control.clicked()
    Keys.onSpacePressed: control.clicked()
}
