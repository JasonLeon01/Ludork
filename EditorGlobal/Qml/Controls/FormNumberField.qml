import QtQuick 2.15
import QtQuick.Controls 2.15

SpinBox {
    id: control

    property var dialogForm: null
    property int decimals: 2
    property real minimumValue: 0.0
    property real maximumValue: 100.0
    property real initialValue: 0.0
    property real singleStep: 1.0
    readonly property int factor: Math.pow(10, decimals)
    readonly property real realValue: value / factor

    from: Math.round(minimumValue * factor)
    to: Math.round(maximumValue * factor)
    stepSize: Math.max(1, Math.round(singleStep * factor))
    value: Math.max(from, Math.min(to, Math.round(initialValue * factor)))
    editable: true
    implicitHeight: 34
    leftPadding: 16
    rightPadding: 35
    font.family: dialogTheme.fontFamily
    font.pixelSize: dialogTheme.fontPixelSize
    palette.text: dialogTheme.textColor
    palette.buttonText: dialogTheme.textColor
    palette.base: dialogTheme.surfaceColor
    palette.highlight: dialogTheme.selectionColor
    palette.highlightedText: dialogTheme.accentTextColor

    validator: DoubleValidator {
        bottom: Math.min(control.minimumValue, control.maximumValue)
        top: Math.max(control.minimumValue, control.maximumValue)
        decimals: control.decimals
        notation: DoubleValidator.StandardNotation
    }

    textFromValue: function(value, locale) {
        return Number(value / control.factor).toLocaleString(locale, "f", control.decimals)
    }

    valueFromText: function(text, locale) {
        var parsed = Number.fromLocaleString(locale, text)
        if (isNaN(parsed))
            return control.value
        return Math.round(parsed * control.factor)
    }

    contentItem: TextInput {
        z: 2
        text: control.displayText
        color: control.enabled ? dialogTheme.textColor : dialogTheme.disabledTextColor
        selectionColor: dialogTheme.selectionColor
        selectedTextColor: dialogTheme.accentTextColor
        font: control.font
        horizontalAlignment: Text.AlignLeft
        verticalAlignment: Text.AlignVCenter
        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
        selectByMouse: true
    }

    up.indicator: Item {
        x: control.mirrored ? 0 : control.width - width
        y: 0
        implicitWidth: 25
        implicitHeight: control.height / 2
        opacity: control.up.pressed ? 0.65 : 1.0

        Image {
            anchors.centerIn: parent
            width: 20
            height: 20
            source: control.enabled && control.value < control.to
                ? dialogTheme.activeUpArrowIcon
                : dialogTheme.disabledUpArrowIcon
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 20
            sourceSize.height: 20
        }
    }

    down.indicator: Item {
        x: control.mirrored ? 0 : control.width - width
        y: control.height - height
        implicitWidth: 25
        implicitHeight: control.height / 2
        opacity: control.down.pressed ? 0.65 : 1.0

        Image {
            anchors.centerIn: parent
            width: 20
            height: 20
            source: control.enabled && control.value > control.from
                ? dialogTheme.activeDownArrowIcon
                : dialogTheme.disabledDownArrowIcon
            fillMode: Image.PreserveAspectFit
            sourceSize.width: 20
            sourceSize.height: 20
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

    WheelHandler {
        onWheel: function(event) {
            if (!control.enabled || event.angleDelta.y === 0)
                return
            var steps = Math.trunc(event.angleDelta.y / 120)
            if (steps === 0)
                return
            if (steps > 0) {
                for (var i = 0; i < steps; ++i)
                    control.increase()
            } else {
                for (var i = 0; i < -steps; ++i)
                    control.decrease()
            }
            event.accepted = true
        }
    }
}
