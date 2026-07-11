import QtQuick 2.15
import QtQuick.Controls 2.15

Rectangle {
    id: root

    property real graphScale: 1.0

    color: "#202124"
    focus: true

    function nodeById(visualId) {
        for (var index = 0; index < referenceGraphNodes.length; ++index) {
            if (referenceGraphNodes[index].visualId === visualId)
                return referenceGraphNodes[index]
        }
        return null
    }

    function fitGraph() {
        var xScale = Math.max(0.2, (graphView.width - 24) / referenceGraphWidth)
        var yScale = Math.max(0.2, (graphView.height - 24) / referenceGraphHeight)
        graphScale = Math.min(1.0, xScale, yScale)
        graphView.contentX = Math.max(0, (referenceGraphWidth * graphScale - graphView.width) / 2)
        graphView.contentY = Math.max(0, (referenceGraphHeight * graphScale - graphView.height) / 2)
    }

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }

    Flickable {
        id: graphView

        anchors.fill: parent
        contentWidth: referenceGraphWidth * root.graphScale
        contentHeight: referenceGraphHeight * root.graphScale
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }
        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        Item {
            id: scaledGraph
            width: referenceGraphWidth
            height: referenceGraphHeight
            scale: root.graphScale
            transformOrigin: Item.TopLeft

            Canvas {
                id: edgeCanvas
                anchors.fill: parent

                onPaint: {
                    var painter = getContext("2d")
                    painter.reset()
                    painter.lineWidth = 2
                    painter.strokeStyle = "#9aa0a6"
                    for (var index = 0; index < referenceGraphEdges.length; ++index) {
                        var edge = referenceGraphEdges[index]
                        var source = root.nodeById(edge.source)
                        var target = root.nodeById(edge.target)
                        if (!source || !target) continue
                        var sourceOnLeft = source.x < target.x
                        var sx = sourceOnLeft ? source.x + source.width : source.x
                        var tx = sourceOnLeft ? target.x : target.x + target.width
                        var sy = source.y + source.height / 2
                        var ty = target.y + target.height / 2
                        var middle = (sx + tx) / 2
                        painter.beginPath()
                        painter.moveTo(sx, sy)
                        painter.lineTo(middle, sy)
                        painter.lineTo(middle, ty)
                        painter.lineTo(tx, ty)
                        painter.stroke()
                    }
                }
            }

            Repeater {
                model: referenceGraphNodes

                Rectangle {
                    x: modelData.x
                    y: modelData.y
                    width: modelData.width
                    height: modelData.height
                    radius: 6
                    color: modelData.color
                    border.width: modelData.current ? 3 : 1
                    border.color: modelData.current ? "#f5c65c" : "#b0b3b8"

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 28
                        radius: 6
                        color: Qt.darker(modelData.color, 1.18)
                        Rectangle {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            height: 6
                            color: parent.color
                        }
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        height: 28
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        text: modelData.title
                        color: modelData.current ? "#ffebb0" : "#ffffff"
                        font.family: dialogTheme.fontFamily
                        font.pixelSize: dialogTheme.fontPixelSize
                        font.bold: true
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideRight
                    }

                    Text {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        anchors.topMargin: 31
                        text: modelData.content
                        color: modelData.current ? "#ffebb0" : "#ebebeb"
                        opacity: 0.9
                        font.family: dialogTheme.fontFamily
                        font.pixelSize: dialogTheme.fontPixelSize
                        verticalAlignment: Text.AlignVCenter
                        elide: Text.ElideMiddle
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: -5
                        width: 10
                        height: 10
                        radius: 5
                        color: "#c5c7ca"
                        border.color: "#55585c"
                    }
                    Rectangle {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.rightMargin: -5
                        width: 10
                        height: 10
                        radius: 5
                        color: "#c5c7ca"
                        border.color: "#55585c"
                    }

                    MouseArea {
                        id: nodeMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: modelData.nodeId.length > 0 ? Qt.PointingHandCursor : Qt.ArrowCursor
                        onDoubleClicked: {
                            if (modelData.nodeId.length > 0)
                                dialogHost.openNode(modelData.nodeId)
                        }
                    }

                    ToolTip.visible: nodeMouse.containsMouse
                    ToolTip.text: modelData.tooltip
                    ToolTip.delay: 350
                }
            }
        }

        WheelHandler {
            onWheel: function(event) {
                var delta = event.angleDelta.y > 0 ? 0.1 : -0.1
                root.graphScale = Math.max(0.2, Math.min(2.0, root.graphScale + delta))
                event.accepted = true
            }
        }
    }

    Component.onCompleted: {
        fitTimer.start()
        forceActiveFocus()
    }

    Timer {
        id: fitTimer
        interval: 0
        repeat: false
        onTriggered: root.fitGraph()
    }
}
