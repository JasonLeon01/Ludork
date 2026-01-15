# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtGui, QtCore
import os
import EditorStatus
from Utils import File, System


class RectCanvas(QtWidgets.QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.image = None
        self.currentRect = QtCore.QRect(0, 0, 0, 0)
        self.dragMode = None
        self.dragStartPos = QtCore.QPoint(0, 0)
        self.dragStartRect = QtCore.QRect(0, 0, 0, 0)

    def setImageAndRect(self, imagePath: str, rectTuple) -> None:
        if imagePath and os.path.exists(imagePath):
            img = QtGui.QImage(imagePath)
            if not img.isNull():
                self.image = img
        if isinstance(rectTuple, tuple) and len(rectTuple) >= 4:
            x, y, w, h = rectTuple[0], rectTuple[1], rectTuple[2], rectTuple[3]
            try:
                x = int(x)
                y = int(y)
                w = int(w)
                h = int(h)
            except Exception:
                x, y, w, h = 0, 0, 0, 0
            self.currentRect = QtCore.QRect(x, y, max(0, w), max(0, h))
        self._clampRectToImage()
        self.update()
        self.adjustSize()

    def getRectTuple(self):
        r = self.currentRect
        return (int(r.x()), int(r.y()), int(r.width()), int(r.height()))

    def _stepSize(self) -> int:
        cell = getattr(EditorStatus, "CELLSIZE", 0)
        if not isinstance(cell, int):
            cell = 0
        step = cell // 2
        if step <= 0:
            step = 1
        return step

    def _snap(self, value: int) -> int:
        step = self._stepSize()
        if step <= 0:
            return int(value)
        return int(round(float(value) / float(step)) * float(step))

    def _clampRectToImage(self) -> None:
        if self.image is None or self.image.isNull():
            return
        iw = self.image.width()
        ih = self.image.height()
        r = self.currentRect
        x = max(0, min(r.x(), iw))
        y = max(0, min(r.y(), ih))
        w = max(0, min(r.width(), iw - x))
        h = max(0, min(r.height(), ih - y))
        if w < 0:
            w = 0
        if h < 0:
            h = 0
        self.currentRect = QtCore.QRect(x, y, w, h)

    def sizeHint(self):
        if self.image and not self.image.isNull():
            return self.image.size()
        return super().sizeHint()

    def minimumSizeHint(self) -> QtCore.QSize:
        return self.sizeHint()

    def paintEvent(self, event):
        p = QtGui.QPainter(self)
        p.fillRect(self.rect(), QtGui.QColor(30, 30, 30))
        if self.image is None or self.image.isNull():
            p.end()
            return
        p.drawImage(QtCore.QPoint(0, 0), self.image)
        r = self.currentRect
        if r.width() > 0 and r.height() > 0:
            fillColor = QtGui.QColor(0, 200, 255, 60)
            borderColor = QtGui.QColor(0, 200, 255)
            p.setBrush(fillColor)
            pen = QtGui.QPen(borderColor)
            pen.setWidth(2)
            p.setPen(pen)
            p.drawRect(r)
        p.end()

    def mousePressEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.image is None or self.image.isNull():
            return
        if e.button() != QtCore.Qt.LeftButton:
            return
        pos = e.pos()
        r = self.currentRect
        if r.width() <= 0 or r.height() <= 0:
            return
        handleSize = 8
        handleRect = QtCore.QRect(r.right() - handleSize + 1, r.bottom() - handleSize + 1, handleSize, handleSize)
        if handleRect.contains(pos):
            self.dragMode = "resize"
        elif r.contains(pos):
            self.dragMode = "move"
        else:
            self.dragMode = None
            return
        self.dragStartPos = QtCore.QPoint(pos)
        self.dragStartRect = QtCore.QRect(self.currentRect)

    def mouseMoveEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.image is None or self.image.isNull():
            return
        if self.dragMode is None:
            return
        pos = e.pos()
        dx = int(pos.x() - self.dragStartPos.x())
        dy = int(pos.y() - self.dragStartPos.y())
        r = QtCore.QRect(self.dragStartRect)
        if self.dragMode == "move":
            nx = self._snap(r.x() + dx)
            ny = self._snap(r.y() + dy)
            self.currentRect = QtCore.QRect(nx, ny, r.width(), r.height())
        elif self.dragMode == "resize":
            nw = self._snap(r.width() + dx)
            nh = self._snap(r.height() + dy)
            if nw < self._stepSize():
                nw = self._stepSize()
            if nh < self._stepSize():
                nh = self._stepSize()
            self.currentRect = QtCore.QRect(r.x(), r.y(), nw, nh)
        self._clampRectToImage()
        self.update()

    def mouseReleaseEvent(self, e: QtGui.QMouseEvent) -> None:
        if e.button() != QtCore.Qt.LeftButton:
            return
        self.dragMode = None


class RectViewer(QtWidgets.QDialog):
    def __init__(self, parent: QtWidgets.QWidget, imagePath: str, rectTuple) -> None:
        super().__init__(parent)
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.Window)
        self.setWindowTitle("Rect Viewer")
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        System.setStyle(self, "config.qss")
        self.canvas = RectCanvas(self)
        self.canvas.setImageAndRect(imagePath, rectTuple)
        self.setMinimumHeight(File.mainWindow.height() // 2)
        self.setMinimumWidth(File.mainWindow.width() // 2)
        self.scrollArea = QtWidgets.QScrollArea(self)
        self.scrollArea.setWidgetResizable(True)
        self.scrollArea.setWidget(self.canvas)
        self.scrollArea.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.scrollArea.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(5, 5, 5, 5)
        layout.addWidget(self.scrollArea, 1)
        btnLayout = QtWidgets.QHBoxLayout()
        btnLayout.addStretch()
        okBtn = QtWidgets.QPushButton("OK")
        cancelBtn = QtWidgets.QPushButton("Cancel")
        okBtn.clicked.connect(self.accept)
        cancelBtn.clicked.connect(self.reject)
        btnLayout.addWidget(okBtn)
        btnLayout.addWidget(cancelBtn)
        layout.addLayout(btnLayout)

    def getRectTuple(self):
        return self.canvas.getRectTuple()
