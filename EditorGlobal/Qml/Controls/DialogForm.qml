import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../dialogLocale.js" as DL

Rectangle {
    id: root

    default property alias formContent: contentColumn.data
    property var resultBuilder: function() { return ({}) }
    property bool confirmEnabled: true
    property bool cancelEnabled: true
    property bool showConfirmButton: true
    property string confirmText: DL.t("CONFIRM", dialogHost)
    property string cancelText: DL.t("CANCEL", dialogHost)
    property int contentSpacing: 8

    color: dialogTheme.backgroundColor
    focus: true

    function requestConfirmation() {
        if (dialogHost && showConfirmButton && confirmEnabled)
            dialogHost.confirm(resultBuilder())
    }

    function requestCancellation() {
        if (dialogHost && cancelEnabled)
            dialogHost.cancel()
    }

    Keys.onEscapePressed: {
        requestCancellation()
        event.accepted = true
    }
    Keys.onReturnPressed: {
        requestConfirmation()
        event.accepted = true
    }
    Keys.onEnterPressed: {
        requestConfirmation()
        event.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        FormScrollView {
            id: scrollView

            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            contentHeight: contentColumn.implicitHeight

            ColumnLayout {
                id: contentColumn

                width: scrollView.availableWidth
                spacing: root.contentSpacing
            }
        }

        DialogActionBar {
            Layout.fillWidth: true
            dialogForm: root
            confirmEnabled: root.confirmEnabled
            cancelEnabled: root.cancelEnabled
            showConfirmButton: root.showConfirmButton
            confirmText: root.confirmText
            cancelText: root.cancelText
        }
    }

    Component.onCompleted: forceActiveFocus()
}
