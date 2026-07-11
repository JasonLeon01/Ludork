import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Forms.DialogForm {
    id: form

    property bool initialising: true

    resultBuilder: function() {
        return {
            "provider": providerCombo.currentText.trim(),
            "model": modelCombo.currentText.trim(),
            "apiKey": keyEdit.text.trim()
        }
    }
    confirmEnabled: providerCombo.currentText.trim().length > 0
        && modelCombo.currentText.trim().length > 0
        && keyEdit.text.trim().length > 0

    function findText(control, value) {
        for (var i = 0; i < control.count; ++i) {
            if (control.textAt(i) === value)
                return i
        }
        return -1
    }

    Forms.FormRow {
        label: DL.t("AI_PROVIDER", dialogHost)

        Forms.FormComboBox {
            id: providerCombo

            Layout.fillWidth: true
            dialogForm: form
            model: aiProviders
            onCurrentTextChanged: {
                if (!form.initialising)
                    modelCombo.currentIndex = modelCombo.count > 0 ? 0 : -1
            }
        }
    }

    Forms.FormRow {
        label: DL.t("AI_MODEL", dialogHost)

        Forms.FormComboBox {
            id: modelCombo

            Layout.fillWidth: true
            dialogForm: form
            model: aiProviderModels[providerCombo.currentText] || []
        }
    }

    Forms.FormRow {
        label: DL.t("API_KEY", dialogHost)

        Forms.FormTextField {
            id: keyEdit

            Layout.fillWidth: true
            dialogForm: form
            text: aiCurrentApiKey
            placeholderText: DL.t("API_KEY", dialogHost)
        }
    }

    Component.onCompleted: {
        var providerIndex = findText(providerCombo, aiCurrentProvider)
        providerCombo.currentIndex = providerIndex >= 0 ? providerIndex : 0
        var modelIndex = findText(modelCombo, aiCurrentModel)
        modelCombo.currentIndex = modelIndex >= 0 ? modelIndex : (modelCombo.count > 0 ? 0 : -1)
        initialising = false
        keyEdit.forceActiveFocus()
    }
}
