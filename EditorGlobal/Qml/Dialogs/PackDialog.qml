import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Forms.DialogForm {
    id: form

    property string selectedPlatform: packDefaultPlatform
    property bool desktopIncludePyAV: false

    resultBuilder: function() {
        return {
            "platform": selectedPlatform,
            "includePyAV": selectedPlatform !== "ios" && includePyAVCheck.checked
        }
    }
    confirmEnabled: selectedPlatform.length > 0

    ButtonGroup {
        id: platformGroup
    }

    Forms.FormLabel {
        Layout.fillWidth: true
        text: DL.t("PACK_MODE_DESC", dialogHost)
    }

    Repeater {
        model: packPlatformOptions

        delegate: Forms.FormRadioButton {
            required property var modelData

            Layout.fillWidth: true
            dialogForm: form
            text: modelData.label
            checked: form.selectedPlatform === modelData.value
            ButtonGroup.group: platformGroup
            onToggled: {
                if (checked)
                    form.selectedPlatform = modelData.value
            }
        }
    }

    Forms.FormCheckBox {
        id: includePyAVCheck

        Layout.fillWidth: true
        dialogForm: form
        text: DL.t("PACK_INCLUDE_PYAV", dialogHost)
        enabled: form.selectedPlatform !== "ios"
        toolTipText: enabled
            ? DL.t("PACK_PYAV_DESKTOP_TIP", dialogHost)
            : DL.t("PACK_PYAV_IOS_TIP", dialogHost)
        onToggled: {
            if (enabled)
                form.desktopIncludePyAV = checked
        }
    }

    onSelectedPlatformChanged: {
        if (selectedPlatform === "ios") {
            desktopIncludePyAV = includePyAVCheck.checked
            includePyAVCheck.checked = false
        } else {
            includePyAVCheck.checked = desktopIncludePyAV
        }
    }
}
