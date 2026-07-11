import QtQuick 2.15
import QtQuick.Controls 2.15

TextField {
    id: control

    property var dialogForm: null
    property string localeHintSuffix: ""
    property bool localeHintEnabled: control.echoMode === TextInput.Normal

    readonly property int localeHintGap: 8
    readonly property color localeHintColor: "#888888"

    function refreshLocaleHint() {
        if (!localeHintEnabled || !dialogHost) {
            localeHintSuffix = ""
            return
        }
        var hint = dialogHost.textInputHintSuffix(control.text, control.cursorPosition)
        localeHintSuffix = hint ? hint : ""
    }

    onTextChanged: refreshLocaleHint()
    onCursorPositionChanged: refreshLocaleHint()
    onActiveFocusChanged: refreshLocaleHint()
    onWidthChanged: refreshLocaleHint()

    Connections {
        target: dialogHost
        function onTextInputHintRefreshRequested() {
            control.refreshLocaleHint()
        }
    }

    FontMetrics {
        id: hintFontMetrics

        font: control.font
    }

    Item {
        enabled: false
        z: 1
        visible: control.localeHintSuffix.length > 0 && control.width > 0
        x: Math.min(
               control.leftPadding + hintFontMetrics.advanceWidth(control.text) + control.localeHintGap,
               Math.max(control.leftPadding, control.width - control.rightPadding - hintLabel.implicitWidth)
           )
        width: Math.max(0, control.width - control.rightPadding - x)
        height: control.height

        Text {
            id: hintLabel

            anchors.verticalCenter: parent.verticalCenter
            width: parent.width
            text: control.localeHintSuffix
            color: control.localeHintColor
            font: control.font
            renderType: Text.NativeRendering
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
    }

    implicitHeight: 34
    leftPadding: 16
    rightPadding: 16
    font.family: dialogTheme.fontFamily
    font.pixelSize: dialogTheme.fontPixelSize
    color: enabled ? dialogTheme.textColor : dialogTheme.disabledTextColor
    placeholderTextColor: dialogTheme.disabledTextColor
    selectionColor: dialogTheme.selectionColor
    selectedTextColor: dialogTheme.accentTextColor
    selectByMouse: true
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

    Component.onCompleted: refreshLocaleHint()
}
