import QtQuick 2.15

FormColourSwatch {
    id: root

    r: colourEditorInitial[0]
    g: colourEditorInitial[1]
    b: colourEditorInitial[2]
    a: colourEditorInitial[3]

    Connections {
        target: colourEditor
        function onQmlValueChanged(red, green, blue, alpha) {
            root.r = red
            root.g = green
            root.b = blue
            root.a = alpha
        }
    }

    onClicked: colourEditor.openPicker()
}
