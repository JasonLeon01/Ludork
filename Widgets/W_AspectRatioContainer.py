# -*- encoding: utf-8 -*-

from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets


class AspectRatioContainer(QtWidgets.QWidget):
    def __init__(self, child: QtWidgets.QWidget, aspectRatio: float, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self.child = child
        self.aspectRatio = float(aspectRatio) if isinstance(aspectRatio, (int, float)) and aspectRatio > 0 else 1.0
        self.child.setParent(self)
        self.child.show()
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        self.setAutoFillBackground(True)
        pal = self.palette()
        pal.setColor(QtGui.QPalette.Window, QtGui.QColor.fromRgb(0, 0, 0))
        self.setPalette(pal)

    def setAspectRatio(self, aspectRatio: float) -> None:
        if not isinstance(aspectRatio, (int, float)) or aspectRatio <= 0:
            return
        self.aspectRatio = float(aspectRatio)
        self._updateChildGeometry()

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self._updateChildGeometry()

    def _updateChildGeometry(self) -> None:
        w = int(self.width())
        h = int(self.height())
        if w <= 0 or h <= 0:
            return
        if self.aspectRatio <= 0:
            self.child.setGeometry(0, 0, w, h)
            return
        containerRatio = float(w) / float(h)
        if containerRatio >= self.aspectRatio:
            targetH = h
            targetW = int(round(float(targetH) * self.aspectRatio))
        else:
            targetW = w
            targetH = int(round(float(targetW) / self.aspectRatio))
        x = int((w - targetW) / 2)
        y = int((h - targetH) / 2)
        self.child.setGeometry(x, y, targetW, targetH)
