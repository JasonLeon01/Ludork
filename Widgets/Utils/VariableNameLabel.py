# -*- encoding: utf-8 -*-

from __future__ import annotations

from PyQt5 import QtCore, QtGui, QtWidgets


class VariableNameLabel(QtWidgets.QLabel):
    def __init__(self, displayName: str, variableName: str, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(displayName, parent)
        self._displayName = displayName
        self._variableName = variableName
        self._timer = QtCore.QTimer(self)
        self._timer.setSingleShot(True)
        self._timer.setInterval(5000)
        self._timer.timeout.connect(self._showDisplayName)
        if self._displayName != self._variableName:
            self.setCursor(QtCore.Qt.PointingHandCursor)

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if self._displayName == self._variableName or event.button() != QtCore.Qt.LeftButton:
            super().mousePressEvent(event)
            return
        if self.text() == self._variableName:
            self._showDisplayName()
        else:
            self.setText(self._variableName)
            self._timer.start()
        event.accept()

    def _showDisplayName(self) -> None:
        self._timer.stop()
        self.setText(self._displayName)
