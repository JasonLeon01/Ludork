import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Forms.DialogForm {
    id: form

    Connections {
        target: dialogHost
        function onBgmPathSelected(path) {
            if (path.length > 0)
                bgmRow.text = path
        }
        function onBgsPathSelected(path) {
            if (path.length > 0)
                bgsRow.text = path
        }
        function onFogPathSelected(path) {
            if (path.length > 0)
                fogRow.text = path
        }
        function onAmbientColourPicked(r, g, b, a) {
            form.ambientR = Math.round(Number(r))
            form.ambientG = Math.round(Number(g))
            form.ambientB = Math.round(Number(b))
            form.ambientA = Math.round(Number(a))
        }
    }

    property int ambientR: 255
    property int ambientG: 255
    property int ambientB: 255
    property int ambientA: 255

    function applyAmbientFromInitial() {
        var light = mapEditInitialData ? mapEditInitialData.ambientLight : null
        if (light && light.length >= 4) {
            ambientR = Math.round(Number(light[0]))
            ambientG = Math.round(Number(light[1]))
            ambientB = Math.round(Number(light[2]))
            ambientA = Math.round(Number(light[3]))
            return
        }
        if (!mapEditInitialData)
            return
        if (mapEditInitialData.ambientR !== undefined)
            ambientR = Math.round(Number(mapEditInitialData.ambientR))
        if (mapEditInitialData.ambientG !== undefined)
            ambientG = Math.round(Number(mapEditInitialData.ambientG))
        if (mapEditInitialData.ambientB !== undefined)
            ambientB = Math.round(Number(mapEditInitialData.ambientB))
        if (mapEditInitialData.ambientA !== undefined)
            ambientA = Math.round(Number(mapEditInitialData.ambientA))
    }

    resultBuilder: function() {
        var fmt = mapEditShowDataFormat ? formatCombo.selectedFormat : null
        return {
            "fileName": fileNameField.text.trim(),
            "dataFormat": fmt,
            "mapName": mapNameField.text.trim(),
            "width": widthSpin.realValue,
            "height": heightSpin.realValue,
            "ambientLight": [form.ambientR, form.ambientG, form.ambientB, form.ambientA],
            "bgm": bgmRow.text.trim(),
            "bgs": bgsRow.text.trim(),
            "fog": fogRow.text.trim(),
            "fogPower": fogPowerSpin.realValue,
            "fogOx": fogOxSpin.realValue,
            "fogOy": fogOySpin.realValue,
            "fogDistort": fogDistortSpin.realValue,
        }
    }

    confirmEnabled: fileNameField.text.trim().length > 0

    Forms.FormRow {
        label: DL.t("FILE_NAME", dialogHost)

        Forms.FormTextField {
            id: fileNameField

            Layout.fillWidth: true
            dialogForm: form
            text: mapEditInitialData.fileName || ""
        }
    }

    Forms.FormRow {
        visible: mapEditShowDataFormat
        label: DL.t("DATA_FORMAT", dialogHost)

        Forms.FormComboBox {
            id: formatCombo

            property string selectedFormat: model.length > 0 && currentIndex >= 0 ? model[currentIndex].value : "json"

            Layout.fillWidth: true
            dialogForm: form
            model: mapEditDataFormats
            textRole: "label"

            onCurrentIndexChanged: {
                var name = fileNameField.text.trim()
                if (!name) return
                var dot = name.lastIndexOf(".")
                if (dot !== -1) name = name.substring(0, dot)
                fileNameField.text = name + (selectedFormat === "json" ? ".json" : ".dat")
            }
        }
    }

    Forms.FormRow {
        label: DL.t("EDIT_MAP", dialogHost)

        Forms.FormTextField {
            id: mapNameField

            Layout.fillWidth: true
            dialogForm: form
            text: mapEditInitialData.mapName || ""
        }
    }

    Forms.FormRow {
        label: DL.t("MAP_WIDTH", dialogHost)

        Forms.FormNumberField {
            id: widthSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 0
            minimumValue: 1
            maximumValue: 32768
            singleStep: 1
            initialValue: mapEditInitialData.width || 20
        }
    }

    Forms.FormRow {
        label: DL.t("MAP_HEIGHT", dialogHost)

        Forms.FormNumberField {
            id: heightSpin

            Layout.fillWidth: true
            dialogForm: form
            decimals: 0
            minimumValue: 1
            maximumValue: 32768
            singleStep: 1
            initialValue: mapEditInitialData.height || 15
        }
    }

    Forms.FormRow {
        label: DL.t("AMBIENT_LIGHT", dialogHost)

        Forms.FormColourSwatch {
            r: form.ambientR
            g: form.ambientG
            b: form.ambientB
            a: form.ambientA

            onClicked: {
                dialogHost.pickAmbientColour(
                    form.ambientR,
                    form.ambientG,
                    form.ambientB,
                    form.ambientA
                )
            }
        }
    }

    Forms.FormRow {
        label: DL.t("MAP_BGM", dialogHost)

        Forms.FormFilePathRow {
            id: bgmRow

            Layout.fillWidth: true
            showFilter: true
            text: mapEditInitialData.bgm || ""

            onBrowse: {
                dialogHost.browseBgm()
            }

            onClear: {
                bgmRow.text = ""
            }

            onFilter: {
                dialogHost.editBgmFilter()
            }
        }
    }

    Forms.FormRow {
        label: DL.t("MAP_BGS", dialogHost)

        Forms.FormFilePathRow {
            id: bgsRow

            Layout.fillWidth: true
            showFilter: true
            text: mapEditInitialData.bgs || ""

            onBrowse: {
                dialogHost.browseBgs()
            }

            onClear: {
                bgsRow.text = ""
            }

            onFilter: {
                dialogHost.editBgsFilter()
            }
        }
    }

    Forms.FormRow {
        label: DL.t("MAP_FOG", dialogHost)

        Forms.FormFilePathRow {
            id: fogRow

            Layout.fillWidth: true
            showFilter: false
            text: mapEditInitialData.fog || ""

            onBrowse: {
                dialogHost.browseFog()
            }

            onClear: {
                fogRow.text = ""
            }
        }
    }

    ColumnLayout {
        visible: fogRow.text.trim().length > 0
        Layout.fillWidth: true
        spacing: 8

        Forms.FormRow {
            label: DL.t("MAP_FOG_POWER", dialogHost)

            Forms.FormNumberField {
                id: fogPowerSpin

                Layout.fillWidth: true
                dialogForm: form
                decimals: 0
                minimumValue: 0
                maximumValue: 100
                singleStep: 1
                initialValue: mapEditInitialData.fogPower || 0
            }
        }

        Forms.FormRow {
            label: DL.t("MAP_FOG_OX", dialogHost)

            Forms.FormNumberField {
                id: fogOxSpin

                Layout.fillWidth: true
                dialogForm: form
                decimals: 2
                minimumValue: -9999
                maximumValue: 9999
                singleStep: 1
                initialValue: mapEditInitialData.fogOx || 0
            }
        }

        Forms.FormRow {
            label: DL.t("MAP_FOG_OY", dialogHost)

            Forms.FormNumberField {
                id: fogOySpin

                Layout.fillWidth: true
                dialogForm: form
                decimals: 2
                minimumValue: -9999
                maximumValue: 9999
                singleStep: 1
                initialValue: mapEditInitialData.fogOy || 0
            }
        }

        Forms.FormRow {
            label: DL.t("MAP_FOG_DISTORT", dialogHost)

            Forms.FormNumberField {
                id: fogDistortSpin

                Layout.fillWidth: true
                dialogForm: form
                decimals: 0
                minimumValue: 0
                maximumValue: 100
                singleStep: 1
                initialValue: mapEditInitialData.fogDistort || 0
            }
        }
    }

    Component.onCompleted: {
        applyAmbientFromInitial()
        fileNameField.forceActiveFocus()
    }
}
