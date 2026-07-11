import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../dialogLocale.js" as DL

RowLayout {
    id: root

    default property alias leadingActions: leadingLayout.data

    property var dialogForm: null
    property bool confirmEnabled: true
    property bool cancelEnabled: true
    property bool showConfirmButton: true
    property string confirmText: DL.t("CONFIRM", dialogHost)
    property string cancelText: DL.t("CANCEL", dialogHost)

    signal confirmClicked()
    signal cancelClicked()

    spacing: 8

    Item {
        Layout.fillWidth: true
    }

    RowLayout {
        id: leadingLayout

        spacing: 8
    }

    DialogButton {
        visible: root.showConfirmButton
        enabled: root.confirmEnabled
        highlighted: true
        text: root.confirmText
        onClicked: {
            if (root.dialogForm)
                root.dialogForm.requestConfirmation()
            else
                root.confirmClicked()
        }
    }

    DialogButton {
        enabled: root.cancelEnabled
        text: root.cancelText
        onClicked: {
            if (root.dialogForm)
                root.dialogForm.requestCancellation()
            else
                root.cancelClicked()
        }
    }
}
