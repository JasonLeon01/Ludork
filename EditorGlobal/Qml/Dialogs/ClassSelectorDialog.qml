import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Rectangle {
    id: root

    property string currentSelection: classSelectorInitialSelection

    color: dialogTheme.backgroundColor
    focus: true

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Forms.FormTextField {
            id: searchField

            Layout.fillWidth: true
            placeholderText: DL.t("SEARCH", dialogHost)
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                Forms.FormLabel {
                    text: DL.t("PROJECT_CLASSES", dialogHost)
                }

                Forms.FormFilterList {
                    id: classesList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: classSelectorClasses
                    filterText: searchField.text
                    onCurrentSelectionChanged: {
                        if (currentSelection.length > 0) {
                            blueprintsList.currentSelection = ""
                            root.currentSelection = currentSelection
                        } else if (blueprintsList.currentSelection.length === 0) {
                            root.currentSelection = ""
                        }
                    }
                    onSelectionConfirmed: function(value) {
                        root.currentSelection = value
                        dialogHost.confirm({ "selected": value })
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 4

                Forms.FormLabel {
                    text: DL.t("PROJECT_BLUEPRINT", dialogHost)
                }

                Forms.FormFilterList {
                    id: blueprintsList

                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: classSelectorBlueprints
                    filterText: searchField.text
                    currentSelection: ""
                    onCurrentSelectionChanged: {
                        if (currentSelection.length > 0) {
                            classesList.currentSelection = ""
                            root.currentSelection = currentSelection
                        } else if (classesList.currentSelection.length === 0) {
                            root.currentSelection = ""
                        }
                    }
                    onSelectionConfirmed: function(value) {
                        root.currentSelection = value
                        dialogHost.confirm({ "selected": value })
                    }
                }
            }
        }

        Forms.DialogActionBar {
            Layout.fillWidth: true
            confirmEnabled: root.currentSelection.length > 0
            confirmText: DL.t("CONFIRM", dialogHost)
            cancelText: DL.t("CANCEL", dialogHost)
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

    Component.onCompleted: {
        if (classSelectorInitialSelection.length > 0) {
            root.currentSelection = classSelectorInitialSelection
            if (classSelectorBlueprints.indexOf(classSelectorInitialSelection) >= 0)
                blueprintsList.currentSelection = classSelectorInitialSelection
            else
                classesList.currentSelection = classSelectorInitialSelection
        }
        searchField.forceActiveFocus()
    }
}
