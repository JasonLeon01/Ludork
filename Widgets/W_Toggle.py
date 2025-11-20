# -*- encoding: utf-8 -*-

from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale


class ModeToggle(QtWidgets.QWidget):
    selectionChanged = QtCore.pyqtSignal(int)

    def __init__(self, scale: float, parent=None):
        super().__init__(parent)
        self._scale = scale
        self._selected = 0
        self.setFixedSize(int(128 * scale), int(32 * scale))
        self.setMouseTracking(True)

    def sizeHint(self):
        return QtCore.QSize(int(128 * self._scale), int(32 * self._scale))

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
        pen = QtGui.QPen(QtGui.QColor(120, 200, 255))
        pen.setWidth(max(1, int(2 * self._scale)))
        p.setPen(pen)
        sel = rectL if self._selected == 0 else rectR
        r = QtCore.QRect(sel)
        r.adjust(int(2 * self._scale), int(2 * self._scale), -int(2 * self._scale), -int(2 * self._scale))
        p.drawRoundedRect(r, int(8 * self._scale), int(8 * self._scale))
        style = QtWidgets.QApplication.style()
        editorIcon = style.standardIcon(QtWidgets.QStyle.SP_FileIcon)
        playIcon = style.standardIcon(QtWidgets.QStyle.SP_MediaPlay)
        iconSize = QtCore.QSize(int(32 * self._scale), int(32 * self._scale))
        ep = editorIcon.pixmap(iconSize)
        pp = playIcon.pixmap(iconSize)
        p.drawPixmap(rectL.center().x() - ep.width() // 2, rectL.center().y() - ep.height() // 2, ep)
        p.drawPixmap(rectR.center().x() - pp.width() // 2, rectR.center().y() - pp.height() // 2, pp)

    def mousePressEvent(self, e):
        idx = 0 if e.x() < self.width() // 2 else 1
        if idx != self._selected:
            self._selected = idx
            self.update()
            self.selectionChanged.emit(idx)

    def mouseMoveEvent(self, e):
        idx = 0 if e.x() < self.width() // 2 else 1
        text = Locale.getContent("EditMap") if idx == 0 else Locale.getContent("TestGame")
        QtWidgets.QToolTip.showText(e.globalPos(), text, self)
        super().mouseMoveEvent(e)

    def event(self, e):
        if e.type() == QtCore.QEvent.ToolTip:
            idx = 0 if e.pos().x() < self.width() // 2 else 1
            text = Locale.getContent("EditMap") if idx == 0 else Locale.getContent("TestGame")
            QtWidgets.QToolTip.showText(e.globalPos(), text, self)
            return True
        return super().event(e)

    def leaveEvent(self, e):
        QtWidgets.QToolTip.hideText()
        e.accept()
