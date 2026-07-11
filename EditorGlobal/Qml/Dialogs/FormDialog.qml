import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms
import "../dialogLocale.js" as DL

Forms.DialogForm {
    id: form

    property var formValues: ({})

    resultBuilder: function() {
        return formValues
    }

    function fieldTooltip(fieldData) {
        if (fieldData.tooltipKey)
            return DL.t(fieldData.tooltipKey, dialogHost)
        if (fieldData.tooltipKeys && fieldData.tooltipSourceField) {
            var sourceValue = formValues[fieldData.tooltipSourceField] || ""
            var linkedKey = fieldData.tooltipKeys[sourceValue] || ""
            return linkedKey ? DL.t(linkedKey, dialogHost) : ""
        }
        if (fieldData.tooltipKeys && fieldData.type === "combo") {
            var comboKey = fieldData.tooltipKeys[formValues[fieldData.name] || ""] || ""
            return comboKey ? DL.t(comboKey, dialogHost) : ""
        }
        return ""
    }

    function initFormValues() {
        var out = {}
        for (var i = 0; i < formFields.length; ++i) {
            var field = formFields[i]
            out[field.name] = field.initialValue !== undefined ? field.initialValue : ""
        }
        formValues = out
    }

    property bool allFieldsAcceptable: {
        var ok = true
        for (var i = 0; i < formFields.length; ++i) {
            var field = formFields[i]
            if (field.type === "int" || field.type === "float" || field.type === "number") {
                var editor = fieldEditors[field.name]
                if (editor && !editor.acceptableInput)
                    ok = false
            }
        }
        return ok
    }

    property var fieldEditors: ({})

    confirmEnabled: allFieldsAcceptable

    Component.onCompleted: initFormValues()

    Repeater {
        model: formFields

        Forms.FormRow {
            label: modelData.label

            Forms.FormTextField {
                id: textEditor

                visible: modelData.type === "text" || modelData.type === "int"
                        || modelData.type === "float" || modelData.type === "number"
                Layout.fillWidth: true
                dialogForm: form
                text: form.formValues[modelData.name] || ""
                readOnly: modelData.readOnly === true
                validator: {
                    if (modelData.type === "int")
                        return intFieldValidator
                    if (modelData.type === "float" || modelData.type === "number")
                        return floatFieldValidator
                    return null
                }
                ToolTip.visible: hovered && form.fieldTooltip(modelData).length > 0
                ToolTip.text: form.fieldTooltip(modelData)
                onTextChanged: {
                    var next = form.formValues
                    next[modelData.name] = text
                    form.formValues = next
                }
                onAcceptableInputChanged: form.fieldEditors = form.fieldEditors
                Component.onCompleted: {
                    form.fieldEditors[modelData.name] = textEditor
                    if (modelData.type === "int") {
                        intFieldValidator.bottom = Math.round(modelData.minValue !== undefined ? modelData.minValue : 0)
                        intFieldValidator.top = Math.round(modelData.maxValue !== undefined ? modelData.maxValue : 999999)
                    } else if (modelData.type === "float" || modelData.type === "number") {
                        floatFieldValidator.bottom = modelData.minValue !== undefined ? modelData.minValue : 0
                        floatFieldValidator.top = modelData.maxValue !== undefined ? modelData.maxValue : 999999
                        floatFieldValidator.decimals = modelData.decimals !== undefined ? modelData.decimals : 6
                    }
                    if (modelData.focus === true) {
                        forceActiveFocus()
                        selectAll()
                    }
                }

                IntValidator {
                    id: intFieldValidator
                }

                DoubleValidator {
                    id: floatFieldValidator
                    notation: DoubleValidator.StandardNotation
                }
            }

            Forms.FormComboBox {
                visible: modelData.type === "combo"
                Layout.fillWidth: true
                dialogForm: form
                model: modelData.options || []
                currentIndex: {
                    var options = modelData.options || []
                    var value = form.formValues[modelData.name] || ""
                    var index = options.indexOf(value)
                    return index >= 0 ? index : (modelData.currentIndex !== undefined ? modelData.currentIndex : 0)
                }
                ToolTip.visible: hovered && form.fieldTooltip(modelData).length > 0
                ToolTip.text: form.fieldTooltip(modelData)
                onCurrentTextChanged: {
                    var next = form.formValues
                    next[modelData.name] = currentText
                    form.formValues = next
                }
                Component.onCompleted: {
                    if (modelData.focus === true)
                        forceActiveFocus()
                }
            }
        }
    }
}
