import QtQuick 2.15
import QtQuick.Controls 2.15

CheckBox {
    id: control

    property var dialogForm: null
    property string toolTipText: ""

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
            radius: 4
        }

        Image {
            anchors.fill: parent
            source: control.enabled
                ? (control.checkState === Qt.Checked
                    ? dialogTheme.primaryCheckedBoxIcon
                    : (control.checkState === Qt.PartiallyChecked
                        ? dialogTheme.primaryIndeterminateBoxIcon
                        : dialogTheme.primaryUncheckedBoxIcon))
                : (control.checkState === Qt.Checked
                    ? dialogTheme.disabledCheckedBoxIcon
                    : (control.checkState === Qt.PartiallyChecked
                        ? dialogTheme.disabledIndeterminateBoxIcon
                        : dialogTheme.disabledUncheckedBoxIcon))
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

    FormToolTip {
        x: Math.max(0, Math.round((control.width - width) / 2))
        y: control.height
        visible: control.hovered && control.toolTipText.length > 0
        text: control.toolTipText
    }

}
