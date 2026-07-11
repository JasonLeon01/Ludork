import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Rectangle {
    id: root

    property string currentDirectory: dialogHost.currentDirectory
    property bool canGoUp: dialogHost.canGoUp
    property string selectedPath: dialogHost.selectedPath
    property string selectedName: dialogHost.selectedName
    property string previewType: dialogHost.previewType
    property string previewSource: dialogHost.previewSource
    property string previewText: dialogHost.previewText

    color: dialogTheme.backgroundColor
    focus: true

    Connections {
        target: dialogHost
        function onBrowserStateChanged() {
            root.currentDirectory = dialogHost.currentDirectory
            root.canGoUp = dialogHost.canGoUp
            root.selectedPath = dialogHost.selectedPath
            root.selectedName = dialogHost.selectedName
            root.previewType = dialogHost.previewType
            root.previewSource = dialogHost.previewSource
            root.previewText = dialogHost.previewText
            if (!fileSelectorSaveMode || root.selectedName.length > 0)
                fileNameField.text = root.selectedName
        }
    }

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }
    Keys.onReturnPressed: {
        if (confirmButton.enabled)
            confirmButton.clicked()
        event.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Forms.DialogButton {
                Layout.preferredWidth: 38
                enabled: root.canGoUp
                text: "↑"
                onClicked: dialogHost.navigateUp()
            }

            Forms.FormLabel { text: DL.t("FILE_DIALOG_LOOK_IN", dialogHost) }

            Forms.FormTextField {
                Layout.fillWidth: true
                text: root.currentDirectory
                readOnly: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 8

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: dialogTheme.surfaceColor
                border.color: dialogTheme.borderColor
                radius: 3
                clip: true

                GridView {
                    id: fileGrid

                    anchors.fill: parent
                    anchors.margins: 4
                    cellWidth: 160
                    cellHeight: 114
                    model: fileSelectorModel
                    boundsBehavior: Flickable.StopAtBounds
                    clip: true
                    ScrollBar.vertical: ScrollBar { }

                    delegate: Item {
                        width: fileGrid.cellWidth
                        height: fileGrid.cellHeight

                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 3
                            radius: 3
                            color: isSelected
                                ? dialogTheme.focusOverlayColor
                                : (entryMouse.containsMouse ? dialogTheme.alternateSurfaceColor : "transparent")
                            border.width: isSelected ? 2 : 1
                            border.color: isSelected ? dialogTheme.accentColor : "transparent"
                        }

                        Item {
                            anchors.top: parent.top
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 80
                            height: 80

                            Image {
                                anchors.fill: parent
                                anchors.margins: 3
                                visible: isImage
                                source: imageSource
                                asynchronous: true
                                cache: true
                                fillMode: Image.PreserveAspectFit
                                sourceSize.width: 80
                                sourceSize.height: 80
                            }

                            Item {
                                anchors.centerIn: parent
                                width: 54
                                height: 44
                                visible: isDirectory

                                Rectangle {
                                    x: 4
                                    y: 4
                                    width: 24
                                    height: 10
                                    radius: 2
                                    color: dialogTheme.accentColor
                                }
                                Rectangle {
                                    x: 2
                                    y: 10
                                    width: 50
                                    height: 31
                                    radius: 3
                                    color: dialogTheme.accentColor
                                    border.color: Qt.darker(dialogTheme.accentColor, 1.35)
                                }
                            }

                            Item {
                                anchors.centerIn: parent
                                width: 40
                                height: 50
                                visible: !isDirectory && !isImage

                                Rectangle {
                                    anchors.fill: parent
                                    color: dialogTheme.alternateSurfaceColor
                                    border.color: dialogTheme.disabledTextColor
                                    radius: 2
                                }
                                Column {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 8
                                    anchors.topMargin: 13
                                    spacing: 5
                                    Repeater {
                                        model: 4
                                        Rectangle {
                                            width: parent.width
                                            height: 2
                                            color: dialogTheme.disabledTextColor
                                            opacity: 0.65
                                        }
                                    }
                                }
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: detailText.top
                            anchors.margins: 5
                            height: 22
                            text: entryName
                            color: dialogTheme.textColor
                            font.family: dialogTheme.fontFamily
                            font.pixelSize: dialogTheme.fontPixelSize
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            elide: Text.ElideMiddle
                        }

                        Text {
                            id: detailText
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 2
                            height: 14
                            text: entryDetail
                            color: dialogTheme.disabledTextColor
                            font.family: dialogTheme.fontFamily
                            font.pixelSize: Math.max(9, dialogTheme.fontPixelSize - 2)
                            horizontalAlignment: Text.AlignHCenter
                        }

                        MouseArea {
                            id: entryMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: dialogHost.selectRow(index)
                            onDoubleClicked: dialogHost.activateRow(index)
                        }
                    }
                }
            }

            Rectangle {
                Layout.preferredWidth: 260
                Layout.fillHeight: true
                color: dialogTheme.surfaceColor
                border.color: dialogTheme.borderColor
                radius: 3
                clip: true

                Image {
                    anchors.fill: parent
                    anchors.margins: 8
                    visible: root.previewType === "image"
                    source: root.previewSource
                    asynchronous: true
                    fillMode: Image.PreserveAspectFit
                    cache: false
                }

                TextArea {
                    anchors.fill: parent
                    anchors.margins: 8
                    visible: root.previewType === "text"
                    text: root.previewText
                    readOnly: true
                    wrapMode: TextEdit.Wrap
                    selectByMouse: true
                    font.family: "Consolas"
                    font.pixelSize: dialogTheme.fontPixelSize
                    color: dialogTheme.textColor
                    selectionColor: dialogTheme.selectionColor
                    selectedTextColor: dialogTheme.accentTextColor
                    background: Rectangle { color: "transparent" }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Forms.FormLabel { text: DL.t("FILE_NAME", dialogHost) }
            Forms.FormTextField {
                id: fileNameField
                Layout.fillWidth: true
                readOnly: !fileSelectorSaveMode
                text: root.selectedName
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Forms.FormLabel { text: DL.t("FILE_DIALOG_FILE_TYPE", dialogHost) }
            Forms.FormComboBox {
                Layout.fillWidth: true
                model: fileSelectorFilters
                onActivated: dialogHost.setNameFilterIndex(index)
            }
            Item { Layout.preferredWidth: 12 }
            Forms.DialogButton {
                id: confirmButton
                highlighted: true
                enabled: fileSelectorSaveMode ? fileNameField.text.trim().length > 0 : root.selectedPath.length > 0
                text: fileSelectorSaveMode ? DL.t("SAVE", dialogHost) : DL.t("FILE_DIALOG_OPEN", dialogHost)
                onClicked: dialogHost.confirm({
                    "fileName": fileNameField.text.trim(),
                    "selectedPath": root.selectedPath
                })
            }
            Forms.DialogButton {
                text: DL.t("CANCEL", dialogHost)
                onClicked: dialogHost.cancel()
            }
        }
    }

    Component.onCompleted: forceActiveFocus()
}
