import QtQuick 2.15
import "../Controls" as Forms

Rectangle {
    id: root

    color: dialogTheme.backgroundColor
    focus: true

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }
    Keys.onReturnPressed: {
        dialogHost.confirm({})
        event.accepted = true
    }
    Keys.onEnterPressed: {
        dialogHost.confirm({})
        event.accepted = true
    }

    Forms.DialogActionBar {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        onConfirmClicked: dialogHost.confirm({})
        onCancelClicked: dialogHost.cancel()
    }

    Component.onCompleted: forceActiveFocus()
}
