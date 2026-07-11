import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: control

    implicitWidth: Math.max(80, contentItem.implicitWidth + 24)
    implicitHeight: 36
    font.family: dialogTheme.fontFamily
    font.pixelSize: dialogTheme.fontPixelSize

    contentItem: Text {
        text: control.text
        color: control.enabled
            ? (control.down ? dialogTheme.accentTextColor : dialogTheme.accentColor)
            : dialogTheme.disabledTextColor
        font: control.font
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        color: control.down
            ? dialogTheme.accentColor
            : (control.hovered || control.activeFocus ? dialogTheme.focusOverlayColor : dialogTheme.backgroundColor)
        border.width: 2
        border.color: control.enabled ? dialogTheme.accentColor : dialogTheme.borderColor
        radius: 4
        opacity: control.enabled ? 1.0 : 0.55
    }
}
