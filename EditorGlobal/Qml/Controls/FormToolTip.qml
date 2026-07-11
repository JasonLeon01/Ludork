import QtQuick 2.15
import QtQuick.Controls 2.15

ToolTip {
    id: control

    objectName: "formToolTip"
    padding: 4
    margins: 6
    font.family: dialogTheme.fontFamily
    font.pixelSize: dialogTheme.fontPixelSize

    contentItem: Text {
        text: control.text
        color: dialogTheme.textColor
        font: control.font
        wrapMode: Text.WordWrap
        renderType: Text.NativeRendering
    }

    background: Rectangle {
        objectName: "formToolTipBackground"
        color: dialogTheme.alternateSurfaceColor
        border.width: 1
        border.color: dialogTheme.backgroundColor
        radius: 4
    }
}
