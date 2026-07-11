# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
from typing import Any

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal import EditorStatus
from EditorGlobal.QmlDialogHost import QmlDialogHost
from Utils import File


class RectViewer(QmlDialogHost):
    def __init__(self, parent: QtWidgets.QWidget, imagePath: str, rectTuple: Any) -> None:
        image = QtGui.QImage(imagePath) if imagePath and os.path.isfile(imagePath) else QtGui.QImage()
        imageWidth = image.width() if not image.isNull() else 0
        imageHeight = image.height() if not image.isNull() else 0
        initialRect = self._normaliseRect(rectTuple, imageWidth, imageHeight)
        cellSize = getattr(EditorStatus, "CELLSIZE", 0)
        step = cellSize // 2 if isinstance(cellSize, int) else 1
        self._rect = initialRect

        mainWindow = File.mainWindow
        minimumWidth = max(480, mainWindow.width() // 2) if isinstance(mainWindow, QtWidgets.QWidget) else 640
        minimumHeight = max(320, mainWindow.height() // 2) if isinstance(mainWindow, QtWidgets.QWidget) else 420
        super().__init__(
            parent,
            ELOC("RECT_VIEWER_TITLE"),
            QtCore.QSize(minimumWidth, minimumHeight),
            QtCore.QSize(minimumWidth, minimumHeight),
        )
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.Window)
        self.loadQml(
            "Dialogs/RectViewer.qml",
            {
                "rectViewerImageSource": QtCore.QUrl.fromLocalFile(os.path.abspath(imagePath)).toString()
                if not image.isNull()
                else "",
                "rectViewerImageWidth": imageWidth,
                "rectViewerImageHeight": imageHeight,
                "rectViewerInitialRect": list(initialRect),
                "rectViewerStep": max(1, step),
            },
        )

    def getRectTuple(self) -> tuple[int, int, int, int]:
        return self._rect

    def _applyResult(self, result: object) -> bool:
        if not isinstance(result, dict):
            return False
        try:
            self._rect = (
                int(result.get("x", 0)),
                int(result.get("y", 0)),
                max(0, int(result.get("width", 0))),
                max(0, int(result.get("height", 0))),
            )
        except (TypeError, ValueError):
            return False
        return True

    def _normaliseRect(
        self, rectTuple: Any, imageWidth: int, imageHeight: int
    ) -> tuple[int, int, int, int]:
        if isinstance(rectTuple, (list, tuple)) and len(rectTuple) >= 4:
            try:
                x, y, width, height = (int(rectTuple[index]) for index in range(4))
            except (TypeError, ValueError):
                x, y, width, height = 0, 0, 0, 0
        else:
            x, y, width, height = 0, 0, 0, 0
        width = min(max(0, width), imageWidth) if imageWidth else max(0, width)
        height = min(max(0, height), imageHeight) if imageHeight else max(0, height)
        x = max(0, min(x, max(0, imageWidth - width))) if imageWidth else max(0, x)
        y = max(0, min(y, max(0, imageHeight - height))) if imageHeight else max(0, y)
        return x, y, width, height
