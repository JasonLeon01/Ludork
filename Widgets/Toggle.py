# -*- encoding: utf-8 -*-

import os
from typing import Dict
from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import File, Panel


_ICON_CACHE: Dict[str, QtGui.QIcon] = {}


def _accentColor(alpha: int = 255) -> QtGui.QColor:
    color = QtWidgets.QApplication.palette().highlight().color()
    color.setAlpha(alpha)
    return color


def _iconPath(name: str) -> str:
    return os.path.join(File.GetRootPath(), "Resource", "icons", f"{name}.svg")


def _modeIcon(name: str) -> QtGui.QIcon:
    icon = _ICON_CACHE.get(name)
    if icon is None:
        icon = QtGui.QIcon(_iconPath(name))
        _ICON_CACHE[name] = icon
    return icon


def _iconRect(rect: QtCore.QRect) -> QtCore.QRect:
    side = min(22, max(18, int(min(rect.width(), rect.height()) * 0.72)))
    center = rect.center()
    return QtCore.QRect(center.x() - side // 2, center.y() - side // 2, side, side)


def _paintIcon(p: QtGui.QPainter, icon: QtGui.QIcon, rect: QtCore.QRect) -> None:
    icon.paint(p, _iconRect(rect), QtCore.Qt.AlignCenter, QtGui.QIcon.Normal, QtGui.QIcon.Off)


class ModeToggle(QtWidgets.QWidget):
    SELECTION_CHANGED = QtCore.pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._selected = 0
        self._editorIcon = _modeIcon("mode_editor")
        self._playIcon = _modeIcon("mode_play")
        self.setFixedSize(128, 32)
        self.setMouseTracking(True)
        Panel.ApplyDisabledOpacity(self)

    def sizeHint(self):
        return QtCore.QSize(128, 32)

    def setSelected(self, idx: int):
        if idx != self._selected:
            self._selected = idx
            self.update()

    def paintEvent(self, e):
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing, True)
        w = self.width()
        h = self.height()
        half = w // 2
        rectL = QtCore.QRect(0, 0, half, h)
        rectR = QtCore.QRect(half, 0, w - half, h)
        bg = QtGui.QColor(40, 40, 40)
        p.fillRect(rectL, bg)
        p.fillRect(rectR, bg)
        sel = rectL if self._selected == 0 else rectR
        p.fillRect(sel, _accentColor(70))
        p.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 28)))
        p.drawLine(half, 6, half, h - 6)
        _paintIcon(p, self._editorIcon, rectL)
        _paintIcon(p, self._playIcon, rectR)

    def mousePressEvent(self, e):
        idx = 0 if e.x() < self.width() // 2 else 1
        if idx != self._selected:
            self._selected = idx
            self.update()
            self.SELECTION_CHANGED.emit(idx)

    def mouseMoveEvent(self, e):
        idx = 0 if e.x() < self.width() // 2 else 1
        text = ELOC("MAPLIST_EDIT") if idx == 0 else ELOC("TEST_GAME")
        QtWidgets.QToolTip.showText(e.globalPos(), text, self)
        super().mouseMoveEvent(e)

    def event(self, e):
        if e.type() == QtCore.QEvent.ToolTip:
            idx = 0 if e.pos().x() < self.width() // 2 else 1
            text = ELOC("MAPLIST_EDIT") if idx == 0 else ELOC("TEST_GAME")
            QtWidgets.QToolTip.showText(e.globalPos(), text, self)
            return True
        return super().event(e)

    def leaveEvent(self, e):
        QtWidgets.QToolTip.hideText()
        e.accept()

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.ApplyDisabledOpacity(self)
        super().changeEvent(e)


class EditModeToggle(QtWidgets.QWidget):
    SELECTION_CHANGED = QtCore.pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._selected = 0
        self._tileIcon = _modeIcon("mode_tilemap")
        self._lightIcon = _modeIcon("mode_light")
        self._actorIcon = _modeIcon("mode_actor")
        self.setFixedSize(192, 32)
        self.setMouseTracking(True)
        Panel.ApplyDisabledOpacity(self)

    def sizeHint(self):
        return QtCore.QSize(192, 32)

    def setSelected(self, idx: int):
        if idx != self._selected:
            self._selected = idx
            self.update()

    def paintEvent(self, e):
        p = QtGui.QPainter(self)
        p.setRenderHint(QtGui.QPainter.Antialiasing, True)
        w = self.width()
        h = self.height()
        third = max(1, w // 3)
        rectL = QtCore.QRect(0, 0, third, h)
        rectM = QtCore.QRect(third, 0, third, h)
        rectR = QtCore.QRect(third * 2, 0, w - third * 2, h)
        bg = QtGui.QColor(40, 40, 40)
        p.fillRect(rectL, bg)
        p.fillRect(rectM, bg)
        p.fillRect(rectR, bg)
        sel = rectL if self._selected == 0 else (rectM if self._selected == 1 else rectR)
        p.fillRect(sel, _accentColor(70))
        p.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 28)))
        p.drawLine(third, 6, third, h - 6)
        p.drawLine(third * 2, 6, third * 2, h - 6)
        _paintIcon(p, self._tileIcon, rectL)
        _paintIcon(p, self._lightIcon, rectM)
        _paintIcon(p, self._actorIcon, rectR)

    def _idxFromX(self, x: int) -> int:
        w = self.width()
        if w <= 0:
            return 0
        third = max(1, w // 3)
        if x < third:
            return 0
        if x < third * 2:
            return 1
        return 2

    def mousePressEvent(self, e):
        if not self.isEnabled():
            return
        idx = self._idxFromX(e.x())
        if idx != self._selected:
            self._selected = idx
            self.update()
            self.SELECTION_CHANGED.emit(idx)

    def mouseMoveEvent(self, e):
        idx = self._idxFromX(e.x())
        if idx == 0:
            text = ELOC("TILE_MODE")
        elif idx == 1:
            text = ELOC("LIGHT_MODE")
        else:
            text = ELOC("ACTOR_MODE")
        QtWidgets.QToolTip.showText(e.globalPos(), text, self)
        super().mouseMoveEvent(e)

    def event(self, e):
        if e.type() == QtCore.QEvent.ToolTip:
            idx = self._idxFromX(e.pos().x())
            if idx == 0:
                text = ELOC("TILE_MODE")
            elif idx == 1:
                text = ELOC("LIGHT_MODE")
            else:
                text = ELOC("ACTOR_MODE")
            QtWidgets.QToolTip.showText(e.globalPos(), text, self)
            return True
        return super().event(e)

    def leaveEvent(self, e):
        QtWidgets.QToolTip.hideText()
        e.accept()

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.ApplyDisabledOpacity(self)
        super().changeEvent(e)
