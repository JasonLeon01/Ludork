import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import "../Controls" as Forms

Rectangle {
    id: root

    property int rectX: rectViewerInitialRect[0]
    property int rectY: rectViewerInitialRect[1]
    property int rectWidth: rectViewerInitialRect[2]
    property int rectHeight: rectViewerInitialRect[3]
    property string dragMode: ""
    property real dragStartX: 0
    property real dragStartY: 0
    property int startRectX: 0
    property int startRectY: 0
    property int startRectWidth: 0
    property int startRectHeight: 0

    color: dialogTheme.backgroundColor
    focus: true

    function snap(value) {
        return Math.round(Number(value) / rectViewerStep) * rectViewerStep
    }

    function setRect(x, y, width, height) {
        var boundedWidth = Math.max(0, Math.min(Math.round(width), rectViewerImageWidth))
        var boundedHeight = Math.max(0, Math.min(Math.round(height), rectViewerImageHeight))
        rectX = Math.max(0, Math.min(Math.round(x), Math.max(0, rectViewerImageWidth - boundedWidth)))
        rectY = Math.max(0, Math.min(Math.round(y), Math.max(0, rectViewerImageHeight - boundedHeight)))
        rectWidth = boundedWidth
        rectHeight = boundedHeight
    }

    Keys.onEscapePressed: {
        dialogHost.cancel()
        event.accepted = true
    }
    Keys.onReturnPressed: {
        dialogHost.confirm({"x": rectX, "y": rectY, "width": rectWidth, "height": rectHeight})
        event.accepted = true
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 5
        spacing: 8

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e1e"
            border.color: dialogTheme.borderColor
            clip: true

            Flickable {
                id: imageFlick

                anchors.fill: parent
                contentWidth: Math.max(width, rectViewerImageWidth)
                contentHeight: Math.max(height, rectViewerImageHeight)
                boundsBehavior: Flickable.StopAtBounds
                clip: true
                interactive: root.dragMode.length === 0
                ScrollBar.horizontal: ScrollBar { }
                ScrollBar.vertical: ScrollBar { }

                Item {
                    width: imageFlick.contentWidth
                    height: imageFlick.contentHeight

                    Image {
                        x: 0
                        y: 0
                        width: rectViewerImageWidth
                        height: rectViewerImageHeight
                        source: rectViewerImageSource
                        fillMode: Image.Stretch
                        cache: false
                        asynchronous: true
                    }

                    Rectangle {
                        x: root.rectX
                        y: root.rectY
                        width: root.rectWidth
                        height: root.rectHeight
                        visible: width > 0 && height > 0
                        color: "#3c00c8ff"
                        border.color: "#00c8ff"
                        border.width: 2
                    }

                    MouseArea {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        width: rectViewerImageWidth
                        height: rectViewerImageHeight
                        cursorShape: root.dragMode === "resize" ? Qt.SizeFDiagCursor : Qt.CrossCursor
                        preventStealing: true

                        onPressed: {
                            root.dragStartX = mouse.x
                            root.dragStartY = mouse.y
                            root.startRectX = root.rectX
                            root.startRectY = root.rectY
                            root.startRectWidth = root.rectWidth
                            root.startRectHeight = root.rectHeight
                            var onHandle = root.rectWidth > 0 && root.rectHeight > 0
                                && mouse.x >= root.rectX + root.rectWidth - 8
                                && mouse.x <= root.rectX + root.rectWidth
                                && mouse.y >= root.rectY + root.rectHeight - 8
                                && mouse.y <= root.rectY + root.rectHeight
                            var inside = mouse.x >= root.rectX && mouse.x <= root.rectX + root.rectWidth
                                && mouse.y >= root.rectY && mouse.y <= root.rectY + root.rectHeight
                            if (onHandle) {
                                root.dragMode = "resize"
                            } else if (inside && root.rectWidth > 0 && root.rectHeight > 0) {
                                root.dragMode = "move"
                            } else {
                                var width = root.rectWidth > 0 ? root.rectWidth : rectViewerStep
                                var height = root.rectHeight > 0 ? root.rectHeight : rectViewerStep
                                root.setRect(root.snap(mouse.x) - width / 2, root.snap(mouse.y) - height / 2, width, height)
                                root.dragMode = ""
                            }
                        }

                        onPositionChanged: {
                            if (!pressed || root.dragMode.length === 0)
                                return
                            var dx = mouse.x - root.dragStartX
                            var dy = mouse.y - root.dragStartY
                            if (root.dragMode === "move") {
                                root.setRect(
                                    root.snap(root.startRectX + dx),
                                    root.snap(root.startRectY + dy),
                                    root.startRectWidth,
                                    root.startRectHeight
                                )
                            } else {
                                root.setRect(
                                    root.startRectX,
                                    root.startRectY,
                                    Math.max(rectViewerStep, root.snap(root.startRectWidth + dx)),
                                    Math.max(rectViewerStep, root.snap(root.startRectHeight + dy))
                                )
                            }
                        }

                        onReleased: root.dragMode = ""
                        onCanceled: root.dragMode = ""
                    }
                }
            }
        }

        Forms.DialogActionBar {
            Layout.fillWidth: true
            onConfirmClicked: dialogHost.confirm({
                "x": root.rectX,
                "y": root.rectY,
                "width": root.rectWidth,
                "height": root.rectHeight
            })
            onCancelClicked: dialogHost.cancel()
        }
    }

    Component.onCompleted: forceActiveFocus()
}
