import QtQuick 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Forms.DialogForm {
    id: form

    resultBuilder: function() {
        return {
            "offset": offsetSpin.realValue,
            "pitch": pitchSpin.realValue,
            "pan": panSpin.realValue,
            "volume": volumeSpin.realValue,
            "loopStart": filterIsBgm ? loopStartSpin.realValue : 0.0,
            "loopEnd": filterIsBgm ? loopEndSpin.realValue : 0.0
        }
    }

    Forms.FormRow {
        label: DL.t("FILTER_OFFSET", dialogHost)

        Forms.FormNumberField {
            id: offsetSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.0
            maximumValue: 999999.0
            singleStep: 0.1
            initialValue: filterInitialData.offset
        }
    }

    Forms.FormRow {
        label: DL.t("FILTER_PITCH", dialogHost)

        Forms.FormNumberField {
            id: pitchSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.01
            maximumValue: 4.0
            singleStep: 0.05
            initialValue: filterInitialData.pitch
        }
    }

    Forms.FormRow {
        label: DL.t("FILTER_PAN", dialogHost)

        Forms.FormNumberField {
            id: panSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: -1.0
            maximumValue: 1.0
            singleStep: 0.1
            initialValue: filterInitialData.pan
        }
    }

    Forms.FormRow {
        label: DL.t("FILTER_VOLUME", dialogHost)

        Forms.FormNumberField {
            id: volumeSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.0
            maximumValue: 100.0
            singleStep: 1.0
            initialValue: filterInitialData.volume
        }
    }

    Forms.FormRow {
        visible: filterIsBgm
        label: DL.t("FILTER_LOOP_POINT", dialogHost)

        Forms.FormNumberField {
            id: loopStartSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.0
            maximumValue: 999999.0
            singleStep: 0.1
            initialValue: filterInitialData.loopStart
        }

        Forms.FormLabel {
            text: "/"
        }

        Forms.FormNumberField {
            id: loopEndSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 2
            minimumValue: 0.0
            maximumValue: 999999.0
            singleStep: 0.1
            initialValue: filterInitialData.loopEnd
        }
    }
}
