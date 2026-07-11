import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Forms.DialogForm {
    id: form

    resultBuilder: function() {
        return {
            "script": gameConfigInitialData.script,
            "language": languageCombo.currentText.trim(),
            "scale": Number(scaleCombo.currentText),
            "framerate": Number(framerateCombo.currentText),
            "verticalsync": verticalSyncCheck.checked,
            "musicon": musicOnCheck.checked,
            "soundon": soundOnCheck.checked,
            "voiceon": voiceOnCheck.checked,
            "musicvolume": musicVolumeSpin.realValue,
            "soundvolume": soundVolumeSpin.realValue,
            "voicevolume": voiceVolumeSpin.realValue
        }
    }
    confirmEnabled: languageCombo.currentText.trim().length > 0

    function selectText(control, value) {
        for (var i = 0; i < control.count; ++i) {
            if (control.textAt(i) === value) {
                control.currentIndex = i
                return
            }
        }
    }

    Forms.FormRow {
        label: DL.t("script", dialogHost)

        Forms.FormTextField {
            Layout.fillWidth: true
            dialogForm: form
            text: gameConfigInitialData.script
            readOnly: true
        }
    }

    Forms.FormRow {
        label: DL.t("language", dialogHost)

        Forms.FormComboBox {
            id: languageCombo

            Layout.fillWidth: true
            dialogForm: form
            model: gameConfigLanguages
        }
    }

    Forms.FormRow {
        label: DL.t("scale", dialogHost)

        Forms.FormComboBox {
            id: scaleCombo

            Layout.fillWidth: true
            dialogForm: form
            model: gameConfigScales
        }
    }

    Forms.FormRow {
        label: DL.t("framerate", dialogHost)

        Forms.FormComboBox {
            id: framerateCombo

            Layout.fillWidth: true
            dialogForm: form
            model: gameConfigFrameRates
        }
    }

    Forms.FormRow {
        label: DL.t("verticalsync", dialogHost)

        Forms.FormCheckBox {
            id: verticalSyncCheck

            dialogForm: form
            checked: gameConfigInitialData.verticalsync
        }
    }

    Forms.FormRow {
        label: DL.t("musicon", dialogHost)

        Forms.FormCheckBox {
            id: musicOnCheck

            dialogForm: form
            checked: gameConfigInitialData.musicon
        }
    }

    Forms.FormRow {
        label: DL.t("soundon", dialogHost)

        Forms.FormCheckBox {
            id: soundOnCheck

            dialogForm: form
            checked: gameConfigInitialData.soundon
        }
    }

    Forms.FormRow {
        label: DL.t("voiceon", dialogHost)

        Forms.FormCheckBox {
            id: voiceOnCheck

            dialogForm: form
            checked: gameConfigInitialData.voiceon
        }
    }

    Forms.FormRow {
        label: DL.t("musicvolume", dialogHost)

        Forms.FormNumberField {
            id: musicVolumeSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.0
            maximumValue: 100.0
            singleStep: 1.0
            initialValue: gameConfigInitialData.musicvolume
        }
    }

    Forms.FormRow {
        label: DL.t("soundvolume", dialogHost)

        Forms.FormNumberField {
            id: soundVolumeSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.0
            maximumValue: 100.0
            singleStep: 1.0
            initialValue: gameConfigInitialData.soundvolume
        }
    }

    Forms.FormRow {
        label: DL.t("voicevolume", dialogHost)

        Forms.FormNumberField {
            id: voiceVolumeSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.0
            maximumValue: 100.0
            singleStep: 1.0
            initialValue: gameConfigInitialData.voicevolume
        }
    }

    Component.onCompleted: {
        selectText(languageCombo, gameConfigInitialData.language)
        selectText(scaleCombo, Number(gameConfigInitialData.scale).toFixed(2))
        selectText(framerateCombo, String(gameConfigInitialData.framerate))
        languageCombo.forceActiveFocus()
    }
}
