import QtQuick 2.15
import QtQuick.Controls 2.15

ComboBox {
    id: control

    property var dialogForm: null

    LayoutMirroring.enabled: false
    implicitHeight: 34
    leftPadding: 16
    rightPadding: 36
    font.family: dialogTheme.fontFamily
    font.pixelSize: dialogTheme.fontPixelSize
    palette.text: dialogTheme.textColor
    palette.buttonText: dialogTheme.textColor
    palette.base: dialogTheme.surfaceColor
    palette.window: dialogTheme.backgroundColor
    palette.highlight: dialogTheme.selectionColor
    palette.highlightedText: dialogTheme.accentTextColor

    delegate: ItemDelegate {
        width: ListView.view.width
        height: 36
        hoverEnabled: true
        highlighted: control.highlightedIndex === index

        contentItem: Text {
            leftPadding: 12
            rightPadding: 12
            text: control.textRole
                ? (Array.isArray(control.model) ? modelData[control.textRole] : model[control.textRole])
                : modelData
            color: dialogTheme.textColor
            font.family: dialogTheme.fontFamily
            font.pixelSize: dialogTheme.fontPixelSize
            font.weight: control.currentIndex === index ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            color: parent.highlighted || parent.hovered
                ? dialogTheme.focusOverlayColor
                : dialogTheme.surfaceColor
        }
    }

    indicator: Image {
        x: control.width - width - 10
        y: Math.round((control.height - height) / 2)
        width: 20
        height: 20
        source: control.enabled
            ? (control.activeFocus ? dialogTheme.primaryDownArrowIcon : dialogTheme.activeDownArrowIcon)
            : dialogTheme.disabledDownArrowIcon
        fillMode: Image.PreserveAspectFit
        sourceSize.width: 20
        sourceSize.height: 20
    }

    contentItem: Text {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        color: control.enabled ? dialogTheme.textColor : dialogTheme.disabledTextColor
        font: control.font
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        renderType: Text.NativeRendering
    }

    Keys.onReturnPressed: {
        if (popup.visible) {
            event.accepted = false
        } else {
            event.accepted = dialogForm !== null
            if (dialogForm)
                dialogForm.requestConfirmation()
        }
    }
    Keys.onEnterPressed: {
        if (popup.visible) {
            event.accepted = false
        } else {
            event.accepted = dialogForm !== null
            if (dialogForm)
                dialogForm.requestConfirmation()
        }
    }

    popup: Popup {
        objectName: "formComboPopup"
        y: control.height - 1
        width: control.width
        height: Math.min(contentItem.implicitHeight, 300)
        padding: 2

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            ScrollIndicator.vertical: ScrollIndicator {
                id: scrollIndicator

                contentItem: Rectangle {
                    implicitWidth: 5
                    radius: 2
                    color: scrollIndicator.active ? dialogTheme.accentColor : dialogTheme.focusOverlayColor
                }
            }
        }

        background: Rectangle {
            objectName: "formComboPopupBackground"
            color: dialogTheme.surfaceColor
            border.width: 2
            border.color: dialogTheme.alternateSurfaceColor
            radius: 4
        }
    }

    background: Rectangle {
        color: control.enabled ? dialogTheme.surfaceColor : dialogTheme.disabledSurfaceColor
        radius: 4

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 2
            color: control.activeFocus ? dialogTheme.accentColor : dialogTheme.inputBorderColor
        }
    }
}
