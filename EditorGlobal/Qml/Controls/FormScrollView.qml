import QtQuick 2.15
import QtQuick.Controls 2.15

ScrollView {
    id: control

    clip: true
    palette.base: dialogTheme.surfaceColor
    palette.button: dialogTheme.focusOverlayColor
    palette.highlight: dialogTheme.accentColor
    palette.mid: dialogTheme.focusOverlayColor
    ScrollBar.vertical.policy: contentHeight > height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
    ScrollBar.horizontal.policy: contentWidth > width ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
}
