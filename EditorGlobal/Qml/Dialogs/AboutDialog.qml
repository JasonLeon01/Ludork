import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Rectangle {
    id: root

    color: dialogTheme.backgroundColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 12

        Text {
            Layout.fillWidth: true
            text: aboutAppName
            color: dialogTheme.textColor
            font.family: dialogTheme.fontFamily
            font.pixelSize: dialogTheme.fontPixelSize + 14
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            renderType: Text.NativeRendering
        }

        Text {
            Layout.fillWidth: true
            text: aboutVersion
            color: dialogTheme.textColor
            font.family: dialogTheme.fontFamily
            font.pixelSize: dialogTheme.fontPixelSize
            horizontalAlignment: Text.AlignHCenter
            renderType: Text.NativeRendering
        }

        Text {
            Layout.fillWidth: true
            text: DL.t("ABOUT_DESC", dialogHost)
            color: dialogTheme.textColor
            font.family: dialogTheme.fontFamily
            font.pixelSize: dialogTheme.fontPixelSize
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            renderType: Text.NativeRendering
        }

        Text {
            Layout.fillWidth: true
            text: DL.t("ABOUT_COPYRIGHT", dialogHost)
            color: dialogTheme.disabledTextColor
            font.family: dialogTheme.fontFamily
            font.pixelSize: dialogTheme.fontPixelSize
            horizontalAlignment: Text.AlignHCenter
            renderType: Text.NativeRendering
        }

        Item {
            Layout.fillHeight: true
        }

        Forms.DialogActionBar {
            Layout.fillWidth: true
            showConfirmButton: false
            cancelText: DL.t("CLOSE", dialogHost)

            Forms.DialogButton {
                text: DL.t("ABOUT_LICENSES", dialogHost)
                onClicked: dialogHost.openLicenses()
            }

            onCancelClicked: dialogHost.cancel()
        }
    }

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }

    Component.onCompleted: forceActiveFocus()
}
