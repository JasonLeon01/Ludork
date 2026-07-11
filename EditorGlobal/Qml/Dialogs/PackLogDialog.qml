import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Forms.DialogForm {
    id: form

    showConfirmButton: false
    cancelEnabled: dialogHost ? dialogHost.closeEnabled : false
    cancelText: DL.t("CLOSE", dialogHost)

    Forms.FormScrollView {
        Layout.fillWidth: true
        Layout.preferredHeight: 500
        clip: true

        Forms.FormTextArea {
            id: logArea

            width: parent.width
            readOnly: true
            wrapMode: TextEdit.NoWrap
            font.family: "Consolas"
        }
    }

    Connections {
        target: dialogHost

        function onLogAppended(value) {
            logArea.insert(logArea.length, value)
            logArea.cursorPosition = logArea.length
        }
    }
}
