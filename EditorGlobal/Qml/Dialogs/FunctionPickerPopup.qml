import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Rectangle {
    id: root

    color: dialogTheme.backgroundColor
    border.color: dialogTheme.borderColor
    focus: true

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RowLayout {
            Layout.fillWidth: true
            spacing: 0

            Forms.FormTextField {
                id: searchField
                Layout.fillWidth: true
                Layout.preferredWidth: 210
                placeholderText: DL.t("SEARCH", dialogHost)
                onTextChanged: dialogHost.setSearchText(text)
            }

            Forms.FormCheckBox {
                Layout.preferredWidth: 110
                checked: functionPickerContextSensitive
                text: DL.t("CONTEXT_SENSITIVE", dialogHost)
                font.pixelSize: Math.max(9, dialogTheme.fontPixelSize - 1)
                onToggled: dialogHost.setContextSensitive(checked)
            }
        }

        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: dialogTheme.borderColor
        }

        ListView {
            id: treeView

            Layout.fillWidth: true
            Layout.fillHeight: true
            model: functionPickerModel
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { }

            delegate: Rectangle {
                width: treeView.width
                height: 28
                color: rowMouse.containsMouse ? dialogTheme.focusOverlayColor : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 6 + itemDepth * 18
                    anchors.rightMargin: 6
                    spacing: 5

                    Text {
                        Layout.preferredWidth: 12
                        text: isExpandable ? (isExpanded ? "▾" : "▸") : ""
                        color: dialogTheme.disabledTextColor
                        font.pixelSize: dialogTheme.fontPixelSize
                        horizontalAlignment: Text.AlignHCenter
                    }

                    Text {
                        Layout.fillWidth: true
                        text: itemName
                        color: itemPath.length > 0 ? dialogTheme.textColor : dialogTheme.accentColor
                        font.family: dialogTheme.fontFamily
                        font.pixelSize: dialogTheme.fontPixelSize
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        if (isExpandable)
                            dialogHost.toggleRow(index)
                    }
                    onDoubleClicked: dialogHost.activateRow(index)
                }
            }
        }
    }

    Component.onCompleted: searchField.forceActiveFocus()
}
