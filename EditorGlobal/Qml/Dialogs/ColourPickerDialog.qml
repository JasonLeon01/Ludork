import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Rectangle {
    id: root

    property int r: 255
    property int g: 255
    property int b: 255
    property int a: 255
    property int hue: 0
    property int saturation: 0
    property int value: 255
    property bool syncing: false
    property var customColours: colourPickerCustomColours

    color: dialogTheme.backgroundColor
    focus: true

    function clamp(value, minimum, maximum) {
        return Math.max(minimum, Math.min(maximum, Math.round(Number(value))))
    }

    function hsvToRgb(h, s, v) {
        var hh = ((h % 360) + 360) % 360
        var ss = clamp(s, 0, 255) / 255
        var vv = clamp(v, 0, 255) / 255
        var c = vv * ss
        var x = c * (1 - Math.abs((hh / 60) % 2 - 1))
        var m = vv - c
        var rr = 0
        var gg = 0
        var bb = 0
        if (hh < 60) { rr = c; gg = x }
        else if (hh < 120) { rr = x; gg = c }
        else if (hh < 180) { gg = c; bb = x }
        else if (hh < 240) { gg = x; bb = c }
        else if (hh < 300) { rr = x; bb = c }
        else { rr = c; bb = x }
        return [Math.round((rr + m) * 255), Math.round((gg + m) * 255), Math.round((bb + m) * 255)]
    }

    function rgbToHsv(red, green, blue) {
        var rr = clamp(red, 0, 255) / 255
        var gg = clamp(green, 0, 255) / 255
        var bb = clamp(blue, 0, 255) / 255
        var maximum = Math.max(rr, gg, bb)
        var minimum = Math.min(rr, gg, bb)
        var delta = maximum - minimum
        var hh = root.hue
        if (delta > 0) {
            if (maximum === rr) hh = 60 * (((gg - bb) / delta) % 6)
            else if (maximum === gg) hh = 60 * (((bb - rr) / delta) + 2)
            else hh = 60 * (((rr - gg) / delta) + 4)
            if (hh < 0) hh += 360
        }
        return [Math.round(hh), maximum === 0 ? 0 : Math.round(delta / maximum * 255), Math.round(maximum * 255)]
    }

    function hexByte(channel) {
        var text = clamp(channel, 0, 255).toString(16).toUpperCase()
        return text.length < 2 ? "0" + text : text
    }

    function rgbaHex() {
        var text = "#" + hexByte(r) + hexByte(g) + hexByte(b)
        return a === 255 ? text : text + hexByte(a)
    }

    function syncControls() {
        syncing = true
        hSpin.value = hue
        sSpin.value = saturation
        vSpin.value = value
        rSpin.value = r
        gSpin.value = g
        bSpin.value = b
        aSpin.value = a
        hexField.text = rgbaHex()
        syncing = false
    }

    function setRgba(red, green, blue, alpha) {
        r = clamp(red, 0, 255)
        g = clamp(green, 0, 255)
        b = clamp(blue, 0, 255)
        a = clamp(alpha, 0, 255)
        var hsv = rgbToHsv(r, g, b)
        hue = hsv[0]
        saturation = hsv[1]
        value = hsv[2]
        syncControls()
    }

    function applyHsv() {
        var rgb = hsvToRgb(hue, saturation, value)
        r = rgb[0]
        g = rgb[1]
        b = rgb[2]
        syncControls()
    }

    function applyHex() {
        var text = hexField.text.trim()
        if (text.charAt(0) === "#") text = text.substring(1)
        if (text.length !== 6 && text.length !== 8) {
            syncControls()
            return
        }
        var red = parseInt(text.substring(0, 2), 16)
        var green = parseInt(text.substring(2, 4), 16)
        var blue = parseInt(text.substring(4, 6), 16)
        var alpha = text.length === 8 ? parseInt(text.substring(6, 8), 16) : a
        if ([red, green, blue, alpha].some(function(channel) { return isNaN(channel) })) {
            syncControls()
            return
        }
        setRgba(red, green, blue, alpha)
    }

    function paletteColour(entry, preserveAlpha) {
        if (entry === null || entry === undefined) return
        if (typeof entry === "string") {
            var text = entry.charAt(0) === "#" ? entry.substring(1) : entry
            setRgba(parseInt(text.substring(0, 2), 16), parseInt(text.substring(2, 4), 16), parseInt(text.substring(4, 6), 16), a)
            return
        }
        setRgba(entry[0], entry[1], entry[2], preserveAlpha ? a : entry[3])
    }

    Connections {
        target: dialogHost
        function onScreenColourPicked(red, green, blue, alpha) {
            root.setRgba(red, green, blue, alpha)
        }
        function onCustomColoursChanged(colours) {
            root.customColours = colours
        }
    }

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }
    Keys.onReturnPressed: {
        dialogHost.confirm({"r": r, "g": g, "b": b, "a": a})
        event.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 270
                spacing: 6

                Item {
                    id: colourPlane
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumWidth: 240
                    Layout.minimumHeight: 180

                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        gradient: Gradient {
                            orientation: Gradient.Horizontal
                            GradientStop { position: 0; color: "white" }
                            GradientStop { position: 1; color: Qt.hsva(root.hue / 359, 1, 1, 1) }
                        }
                    }
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1
                        gradient: Gradient {
                            GradientStop { position: 0; color: "transparent" }
                            GradientStop { position: 1; color: "black" }
                        }
                    }
                    Rectangle {
                        x: 1 + root.saturation * Math.max(1, parent.width - 3) / 255 - width / 2
                        y: 1 + (255 - root.value) * Math.max(1, parent.height - 3) / 255 - height / 2
                        width: 13
                        height: 13
                        radius: 7
                        color: "transparent"
                        border.width: 3
                        border.color: "black"
                        Rectangle {
                            anchors.fill: parent
                            anchors.margins: 2
                            radius: width / 2
                            color: "transparent"
                            border.width: 1
                            border.color: "white"
                        }
                    }
                    Rectangle { anchors.fill: parent; color: "transparent"; border.color: "#373737" }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.CrossCursor
                        onPressed: updateColour(mouse.x, mouse.y)
                        onPositionChanged: if (pressed) updateColour(mouse.x, mouse.y)
                        function updateColour(x, y) {
                            root.saturation = root.clamp((x - 1) * 255 / Math.max(1, width - 3), 0, 255)
                            root.value = 255 - root.clamp((y - 1) * 255 / Math.max(1, height - 3), 0, 255)
                            root.applyHsv()
                        }
                    }
                }

                Item {
                    id: hueBar
                    Layout.preferredWidth: 28
                    Layout.fillHeight: true
                    Layout.minimumHeight: 180

                    Rectangle {
                        x: 7
                        y: 1
                        width: 13
                        height: parent.height - 3
                        gradient: Gradient {
                            GradientStop { position: 0; color: "#ff0000" }
                            GradientStop { position: 0.1667; color: "#ffff00" }
                            GradientStop { position: 0.3333; color: "#00ff00" }
                            GradientStop { position: 0.5; color: "#00ffff" }
                            GradientStop { position: 0.6667; color: "#0000ff" }
                            GradientStop { position: 0.8333; color: "#ff00ff" }
                            GradientStop { position: 1; color: "#ff0000" }
                        }
                        border.color: "#373737"
                    }
                    Rectangle {
                        x: 2
                        y: 1 + root.hue * Math.max(1, parent.height - 4) / 359 - 1
                        width: parent.width - 4
                        height: 3
                        color: "white"
                        border.color: "black"
                    }
                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onPressed: updateHue(mouse.y)
                        onPositionChanged: if (pressed) updateHue(mouse.y)
                        function updateHue(y) {
                            root.hue = root.clamp((y - 1) * 359 / Math.max(1, height - 4), 0, 359)
                            root.applyHsv()
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.preferredWidth: 181
                Layout.fillHeight: true
                spacing: 8

                Forms.FormLabel { text: DL.t("COLOUR_PICKER_BASIC_COLOURS", dialogHost) }
                GridLayout {
                    columns: 8
                    rowSpacing: 3
                    columnSpacing: 3
                    Repeater {
                        model: colourPickerBasicColours
                        delegate: paletteCell
                    }
                }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_CUSTOM_COLOURS", dialogHost) }
                GridLayout {
                    columns: 8
                    rowSpacing: 3
                    columnSpacing: 3
                    Repeater {
                        model: root.customColours
                        delegate: paletteCell
                    }
                }
                Forms.DialogButton {
                    Layout.fillWidth: true
                    text: DL.t("COLOUR_PICKER_PICK_SCREEN", dialogHost)
                    onClicked: dialogHost.pickScreenColour(root.a)
                }
                Forms.DialogButton {
                    Layout.fillWidth: true
                    text: DL.t("COLOUR_PICKER_ADD_CUSTOM", dialogHost)
                    onClicked: dialogHost.addCustomColour(root.r, root.g, root.b, root.a)
                }
                Item { Layout.fillHeight: true }
            }

            GridLayout {
                columns: 2
                columnSpacing: 8
                rowSpacing: 8

                Forms.FormLabel { text: DL.t("COLOUR_PICKER_CURRENT", dialogHost) }
                Forms.ColourPreview { rgba: colourPickerInitial }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_NEW", dialogHost) }
                Forms.ColourPreview { rgba: [root.r, root.g, root.b, root.a] }

                Forms.FormLabel { text: DL.t("COLOUR_PICKER_HUE", dialogHost) }
                Forms.ColourSpinField { id: hSpin; maximumValue: 359; onEdited: if (!root.syncing) { root.hue = realValue; root.applyHsv() } }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_SATURATION", dialogHost) }
                Forms.ColourSpinField { id: sSpin; onEdited: if (!root.syncing) { root.saturation = realValue; root.applyHsv() } }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_VALUE", dialogHost) }
                Forms.ColourSpinField { id: vSpin; onEdited: if (!root.syncing) { root.value = realValue; root.applyHsv() } }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_RED", dialogHost) }
                Forms.ColourSpinField { id: rSpin; onEdited: if (!root.syncing) root.setRgba(realValue, root.g, root.b, root.a) }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_GREEN", dialogHost) }
                Forms.ColourSpinField { id: gSpin; onEdited: if (!root.syncing) root.setRgba(root.r, realValue, root.b, root.a) }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_BLUE", dialogHost) }
                Forms.ColourSpinField { id: bSpin; onEdited: if (!root.syncing) root.setRgba(root.r, root.g, realValue, root.a) }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_ALPHA", dialogHost) }
                Forms.ColourSpinField { id: aSpin; onEdited: if (!root.syncing) root.setRgba(root.r, root.g, root.b, realValue) }
                Forms.FormLabel { text: DL.t("COLOUR_PICKER_HTML", dialogHost) }
                Forms.FormTextField {
                    id: hexField
                    Layout.preferredWidth: 108
                    maximumLength: 9
                    onEditingFinished: root.applyHex()
                }
            }
        }

        Forms.DialogActionBar {
            Layout.fillWidth: true
            onConfirmClicked: dialogHost.confirm({"r": root.r, "g": root.g, "b": root.b, "a": root.a})
            onCancelClicked: dialogHost.cancel()
        }
    }

    Component {
        id: paletteCell
        Rectangle {
            property var entry: modelData
            width: 20
            height: 20
            color: entry === null || entry === undefined ? "#262626" : "transparent"
            border.color: "#464646"
            clip: true
            Forms.Checkerboard {
                anchors.fill: parent
                anchors.margins: 1
                cellSize: 4
                visible: entry !== null && entry !== undefined
            }
            Rectangle {
                anchors.fill: parent
                anchors.margins: 1
                visible: entry !== null && entry !== undefined
                color: {
                    if (typeof entry === "string") return entry
                    return Qt.rgba(entry[0] / 255, entry[1] / 255, entry[2] / 255, entry[3] / 255)
                }
            }
            Rectangle {
                width: 1
                height: Math.sqrt(parent.width * parent.width + parent.height * parent.height)
                anchors.centerIn: parent
                rotation: -45
                color: "#525252"
                visible: entry === null || entry === undefined
            }
            MouseArea {
                anchors.fill: parent
                enabled: entry !== null && entry !== undefined
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: root.paletteColour(entry, typeof entry === "string")
            }
        }
    }

    Component.onCompleted: {
        setRgba(colourPickerInitial[0], colourPickerInitial[1], colourPickerInitial[2], colourPickerInitial[3])
        forceActiveFocus()
    }
}
