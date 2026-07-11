import QtQuick 2.15
import QtQuick.Layouts 1.15

ColumnLayout {
    id: root

    property var model: []
    property string currentSelection: ""
    property string placeholderText: ""

    signal selectionConfirmed(string value)

    spacing: 6

    FormTextField {
        id: searchField

        Layout.fillWidth: true
        placeholderText: root.placeholderText
    }

    FormFilterList {
        id: filterList

        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 200
        model: root.model
        filterText: searchField.text
        currentSelection: root.currentSelection
        onCurrentSelectionChanged: {
            if (currentSelection !== root.currentSelection)
                root.currentSelection = currentSelection
        }
        onSelectionConfirmed: function(value) {
            root.currentSelection = value
            root.selectionConfirmed(value)
        }
    }

    function forceSearchFocus() {
        searchField.forceActiveFocus()
    }
}
