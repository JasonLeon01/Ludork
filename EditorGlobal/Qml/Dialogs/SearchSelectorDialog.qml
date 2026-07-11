import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Rectangle {
    id: root

    property string currentSelection: searchSelectorInitial

    color: dialogTheme.backgroundColor
    focus: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Forms.FormSearchList {
            id: searchList

            Layout.fillWidth: true
            Layout.fillHeight: true
            model: searchSelectorItems
            currentSelection: root.currentSelection
            placeholderText: DL.t("SEARCH", dialogHost)
            onCurrentSelectionChanged: root.currentSelection = currentSelection
            onSelectionConfirmed: function(value) {
                root.currentSelection = value
                dialogHost.confirm({ "selected": value })
            }
        }

        Forms.DialogActionBar {
            Layout.fillWidth: true
            confirmEnabled: root.currentSelection.length > 0
            onConfirmClicked: dialogHost.confirm({ "selected": root.currentSelection })
            onCancelClicked: dialogHost.cancel()
        }
    }

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }

    Keys.onReturnPressed: {
        if (root.currentSelection.length > 0)
            dialogHost.confirm({ "selected": root.currentSelection })
        event.accepted = true
    }

    Keys.onEnterPressed: {
        if (root.currentSelection.length > 0)
            dialogHost.confirm({ "selected": root.currentSelection })
        event.accepted = true
    }

    Component.onCompleted: searchList.forceSearchFocus()
}
