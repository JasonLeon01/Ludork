import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root

    property var model: []
    property string filterText: ""
    property string currentSelection: ""

    signal selectionConfirmed(string value)

    color: dialogTheme.surfaceColor
    radius: 4
    clip: true

    ListView {
        id: listView

        anchors.fill: parent
        anchors.margins: 2
        model: {
            var needle = root.filterText.trim().toLowerCase()
            if (!needle)
                return root.model
            var filtered = []
            for (var i = 0; i < root.model.length; ++i) {
                if (root.model[i].toLowerCase().indexOf(needle) !== -1)
                    filtered.push(root.model[i])
            }
            return filtered
        }
        currentIndex: {
            for (var i = 0; i < model.length; ++i) {
                if (model[i] === root.currentSelection)
                    return i
            }
            return -1
        }
        highlightMoveDuration: 0
        clip: true

        onCurrentIndexChanged: {
            if (currentIndex >= 0 && currentIndex < model.length)
                root.currentSelection = model[currentIndex]
            else if (currentIndex < 0)
                root.currentSelection = ""
        }

        ScrollBar.vertical: ScrollBar {
            policy: listView.contentHeight > listView.height ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        }

        delegate: ItemDelegate {
            width: listView.width - (listView.ScrollBar.vertical.visible ? 12 : 0)
            height: 32
            hoverEnabled: true
            highlighted: listView.currentIndex === index

            contentItem: Text {
                leftPadding: 10
                rightPadding: 10
                text: modelData
                color: dialogTheme.textColor
                font.family: dialogTheme.fontFamily
                font.pixelSize: dialogTheme.fontPixelSize
                font.weight: listView.currentIndex === index ? Font.DemiBold : Font.Normal
                verticalAlignment: Text.AlignVCenter
                elide: Text.ElideRight
                renderType: Text.NativeRendering
            }

            background: Rectangle {
                color: parent.highlighted || parent.hovered
                    ? dialogTheme.focusOverlayColor
                    : "transparent"
            }

            onClicked: {
                listView.currentIndex = index
            }

            onDoubleClicked: {
                listView.currentIndex = index
                root.selectionConfirmed(modelData)
            }
        }
    }
}
