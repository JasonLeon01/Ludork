import QtQuick 2.15
import QtQuick.Controls 2.15

RadioButton {
    id: control

    property var dialogForm: null

    implicitHeight: 36
    font.family: dialogTheme.fontFamily
    font.pixelSize: dialogTheme.fontPixelSize
    palette.text: dialogTheme.textColor
    palette.buttonText: dialogTheme.textColor
    palette.highlight: dialogTheme.accentColor

    indicator: Item {
        implicitWidth: 28
        implicitHeight: 28
        x: control.leftPadding
        y: Math.round((control.height - height) / 2)

        Rectangle {
            anchors.fill: parent
            visible: control.activeFocus
            color: dialogTheme.focusOverlayColor
            radius: 14
        }

        Image {
            anchors.fill: parent
            source: control.enabled
                ? (control.checked
                    ? dialogTheme.primaryCheckedRadioIcon
                    : dialogTheme.primaryUncheckedRadioIcon)
                : (control.checked
                    ? dialogTheme.disabledCheckedRadioIcon
                    : dialogTheme.disabledUncheckedRadioIcon)
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 28
            sourceSize.height: 28
        }
    }

    contentItem: Text {
        leftPadding: control.indicator.width + control.spacing
        text: control.text
        color: control.enabled ? dialogTheme.textColor : dialogTheme.disabledTextColor
        font: control.font
        verticalAlignment: Text.AlignVCenter
        renderType: Text.NativeRendering
    }

}
