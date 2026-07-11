import QtQuick 2.15
import QtQuick.Layouts 1.15

FormNumberField {
    id: control

    signal edited(real realValue)

    Layout.preferredWidth: 82
    decimals: 0
    minimumValue: 0
    maximumValue: 255
    singleStep: 1

    onValueModified: edited(realValue)
}
