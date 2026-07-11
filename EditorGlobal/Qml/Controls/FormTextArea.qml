import QtQuick 2.15
import QtQuick.Controls 2.15

TextArea {
    id: control

    font.family: dialogTheme.fontFamily
    font.pixelSize: dialogTheme.fontPixelSize
    leftPadding: 16
    rightPadding: 16
    topPadding: 8
    bottomPadding: 8
    color: dialogTheme.textColor
    placeholderTextColor: dialogTheme.disabledTextColor
    selectionColor: dialogTheme.selectionColor
    selectedTextColor: dialogTheme.accentTextColor
    selectByMouse: true
    wrapMode: TextEdit.Wrap

    background: Rectangle {
        color: dialogTheme.surfaceColor
        border.width: 2
        border.color: control.activeFocus ? dialogTheme.accentColor : dialogTheme.borderColor
        radius: 4
    }
}
