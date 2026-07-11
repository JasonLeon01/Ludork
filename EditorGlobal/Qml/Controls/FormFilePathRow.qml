import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../dialogLocale.js" as DL

RowLayout {
    id: root

    property string text: ""
    property bool showClear: true
    property bool showFilter: false

    signal browse()
    signal clear()
    signal filter()

    spacing: 6

    TextField {
        id: pathField

        Layout.fillWidth: true
        implicitHeight: 34
        leftPadding: 16
        rightPadding: 16
        text: root.text
        readOnly: true
        font.family: dialogTheme.fontFamily
        font.pixelSize: dialogTheme.fontPixelSize
        color: dialogTheme.textColor
        selectionColor: dialogTheme.selectionColor
        selectedTextColor: dialogTheme.accentTextColor
        selectByMouse: true
        background: Rectangle {
            color: dialogTheme.disabledSurfaceColor
            radius: 4

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                height: 2
                color: dialogTheme.inputBorderColor
            }
        }
    }

    Button {
        implicitWidth: 34
        implicitHeight: 34
        text: "..."
        font.family: dialogTheme.fontFamily
        font.pixelSize: dialogTheme.fontPixelSize

        contentItem: Text {
            text: parent.text
            color: parent.enabled
                ? (parent.down ? dialogTheme.accentTextColor : dialogTheme.accentColor)
                : dialogTheme.disabledTextColor
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            color: parent.down
                ? dialogTheme.accentColor
                : (parent.hovered || parent.activeFocus ? dialogTheme.focusOverlayColor : dialogTheme.surfaceColor)
            border.width: 2
            border.color: parent.enabled ? dialogTheme.accentColor : dialogTheme.borderColor
            radius: 4
            opacity: parent.enabled ? 1.0 : 0.55
        }

        onClicked: root.browse()
    }

    Button {
        visible: root.showClear
        implicitHeight: 34
        font.family: dialogTheme.fontFamily
        font.pixelSize: dialogTheme.fontPixelSize
        text: DL.t("CLEAR", dialogHost)

        contentItem: Text {
            text: parent.text
            color: parent.enabled
                ? (parent.down ? dialogTheme.accentTextColor : dialogTheme.accentColor)
                : dialogTheme.disabledTextColor
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            color: parent.down
                ? dialogTheme.accentColor
                : (parent.hovered || parent.activeFocus ? dialogTheme.focusOverlayColor : dialogTheme.surfaceColor)
            border.width: 2
            border.color: parent.enabled ? dialogTheme.accentColor : dialogTheme.borderColor
            radius: 4
            opacity: parent.enabled ? 1.0 : 0.55
        }

        onClicked: root.clear()
    }

    Button {
        visible: root.showFilter
        implicitHeight: 34
        font.family: dialogTheme.fontFamily
        font.pixelSize: dialogTheme.fontPixelSize
        text: DL.t("FILTER", dialogHost)

        contentItem: Text {
            text: parent.text
            color: parent.enabled
                ? (parent.down ? dialogTheme.accentTextColor : dialogTheme.accentColor)
                : dialogTheme.disabledTextColor
            font: parent.font
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            renderType: Text.NativeRendering
        }

        background: Rectangle {
            color: parent.down
                ? dialogTheme.accentColor
                : (parent.hovered || parent.activeFocus ? dialogTheme.focusOverlayColor : dialogTheme.surfaceColor)
            border.width: 2
            border.color: parent.enabled ? dialogTheme.accentColor : dialogTheme.borderColor
            radius: 4
            opacity: parent.enabled ? 1.0 : 0.55
        }

        onClicked: root.filter()
    }
}
