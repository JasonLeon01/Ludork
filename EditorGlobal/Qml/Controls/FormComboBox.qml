import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Controls 2.15

ComboBox {
    id: control

    property var dialogForm: null
    readonly property int popupItemHeight: 36

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
        height: control.popupItemHeight
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
        id: popup

        objectName: "formComboPopup"
        width: control.width
        padding: 2
        topMargin: 6
        bottomMargin: 6

        readonly property int wantedHeight: control.count * control.popupItemHeight + topPadding + bottomPadding
        readonly property real comboY: control.mapToItem(null, 0, 0).y
        readonly property real spaceBelow: Math.max(0, control.Window.height - (comboY + control.height) - bottomMargin)
        readonly property real spaceAbove: Math.max(0, comboY - topMargin)
        readonly property real spaceFromComboTop: Math.max(0, control.Window.height - comboY - bottomMargin)
        readonly property bool fitsBelow: wantedHeight <= spaceBelow
        readonly property bool fitsAbove: wantedHeight <= spaceAbove

        y: fitsBelow ? control.height - 1 : (fitsAbove ? 1 - height : 0)
        height: Math.min(
            wantedHeight,
            fitsBelow ? spaceBelow : (fitsAbove ? spaceAbove : spaceFromComboTop),
            300
        )

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            boundsBehavior: Flickable.StopAtBounds
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            highlightMoveDuration: 0

            ScrollBar.vertical: ScrollBar {
                id: scrollBar

                policy: popup.height + 0.5 < popup.wantedHeight ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff

                contentItem: Rectangle {
                    implicitWidth: 5
                    radius: 2
                    color: scrollBar.pressed || scrollBar.hovered
                        ? dialogTheme.accentColor
                        : dialogTheme.focusOverlayColor
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
