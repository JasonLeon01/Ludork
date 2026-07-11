import QtQuick 2.15
import QtQuick.Layouts 1.15

RowLayout {
    id: root

    default property alias fieldContent: fieldContainer.data
    property string label: ""
    property int labelWidth: dialogTheme.formLabelWidth

    Layout.fillWidth: true
    spacing: 8

    FormLabel {
        Layout.preferredWidth: root.labelWidth
        Layout.alignment: Qt.AlignVCenter
        text: root.label
        wrapMode: Text.NoWrap
    }

    RowLayout {
        id: fieldContainer

        Layout.fillWidth: true
        spacing: 6
    }
}
