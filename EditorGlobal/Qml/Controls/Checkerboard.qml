import QtQuick 2.15

Item {
    id: root

    property int cellSize: 5
    property color lightColor: "#d2d2d2"
    property color darkColor: "#969696"

    clip: true

    Grid {
        anchors.fill: parent
        columns: Math.max(1, Math.ceil(root.width / root.cellSize))

        Repeater {
            model: Math.ceil(root.width / root.cellSize) * Math.ceil(root.height / root.cellSize)

            Rectangle {
                width: root.cellSize
                height: root.cellSize
                color: {
                    var columns = Math.max(1, Math.ceil(root.width / root.cellSize))
                    return ((index % columns) + Math.floor(index / columns)) % 2 === 0
                        ? root.lightColor
                        : root.darkColor
                }
            }
        }
    }
}
