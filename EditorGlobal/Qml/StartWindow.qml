import QtQuick 2.15

Rectangle {
    id: root

    property string appName: startAppName
    property string newProjectText: startNewProjectText
    property string openProjectText: startOpenProjectText
    property string uiFontFamily: startFontFamily

    width: 480
    height: 320
    color: "#121212"

    function projectUrl(drag) {
        if (!drag.hasUrls || drag.urls.length === 0)
            return ""
        for (var i = 0; i < drag.urls.length; ++i) {
            var value = drag.urls[i].toString()
            if (value.toLowerCase().slice(-5) === ".proj")
                return value
        }
        return ""
    }

    Text {
        anchors.top: parent.top
        anchors.topMargin: 24
        anchors.horizontalCenter: parent.horizontalCenter
        text: root.appName
        color: "#ffffff"
        font.family: root.uiFontFamily
        font.pointSize: 28
        font.bold: true
        renderType: Text.NativeRendering
        horizontalAlignment: Text.AlignHCenter
    }

    Column {
        x: 24
        y: 149
        width: parent.width - 48
        spacing: 16

        StartActionButton {
            width: parent.width
            label: root.newProjectText
            fontFamily: root.uiFontFamily
            onClicked: startBridge.requestNewProject()
        }

        StartActionButton {
            width: parent.width
            label: root.openProjectText
            fontFamily: root.uiFontFamily
            onClicked: startBridge.requestOpenProject()
        }
    }

    DropArea {
        anchors.fill: parent

        onEntered: drag.accepted = root.projectUrl(drag) !== ""
        onDropped: {
            var url = root.projectUrl(drop)
            if (url !== "") {
                drop.acceptProposedAction()
                startBridge.openDroppedProject(url)
            }
        }
    }
}
