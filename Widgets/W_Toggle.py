# -*- encoding: utf-8 -*-

from PyQt5 import QtCore, QtGui, QtWidgets
from Utils import Locale, Panel


class ModeToggle(QtWidgets.QWidget):
    selectionChanged = QtCore.pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._selected = 0
        self.setFixedSize(128, 32)
        self.setMouseTracking(True)
        Panel.applyDisabledOpacity(self)

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
        self._drawEditorIcon(p, rectL)
        self._drawPlayIcon(p, rectR)
        sel = rectL if self._selected == 0 else rectR
        overlay = QtGui.QColor(255, 255, 255, 80)
        p.fillRect(sel, overlay)

    def mousePressEvent(self, e):
        idx = 0 if e.x() < self.width() // 2 else 1
        if idx != self._selected:
            self._selected = idx
            self.update()
            self.selectionChanged.emit(idx)

    def mouseMoveEvent(self, e):
        idx = 0 if e.x() < self.width() // 2 else 1
        text = Locale.getContent("EDIT_MAP") if idx == 0 else Locale.getContent("TEST_GAME")
        QtWidgets.QToolTip.showText(e.globalPos(), text, self)
        super().mouseMoveEvent(e)

    def event(self, e):
        if e.type() == QtCore.QEvent.ToolTip:
            idx = 0 if e.pos().x() < self.width() // 2 else 1
            text = Locale.getContent("EDIT_MAP") if idx == 0 else Locale.getContent("TEST_GAME")
            QtWidgets.QToolTip.showText(e.globalPos(), text, self)
            return True
        return super().event(e)

    def leaveEvent(self, e):
        QtWidgets.QToolTip.hideText()
        e.accept()

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.applyDisabledOpacity(self)
        super().changeEvent(e)

    def _fgColor(self) -> QtGui.QColor:
        return QtGui.QColor(220, 220, 220)

    def _drawEditorIcon(self, p: QtGui.QPainter, rect: QtCore.QRect):
        s = 30
        cx = rect.center().x()
        cy = rect.center().y()
        x0 = cx - s // 2
        y0 = cy - s // 2
        pen = QtGui.QPen(self._fgColor())
        pen.setWidth(max(1, 2))
        p.setPen(pen)
        p.setBrush(QtCore.Qt.NoBrush)
        r = QtCore.QRect(x0, y0, s, s)
        p.drawRoundedRect(r, 4, 4)
        fold = 6
        path = QtGui.QPainterPath()
        path.moveTo(x0 + s - fold, y0)
        path.lineTo(x0 + s, y0)
        path.lineTo(x0 + s, y0 + fold)
        p.drawPath(path)
        pb = QtGui.QPen(self._fgColor())
        pb.setWidth(max(1, 2))
        p.setPen(pb)
        x1 = x0 + int(s * 0.25)
        y1 = y0 + int(s * 0.70)
        x2 = x0 + int(s * 0.85)
        y2 = y0 + int(s * 0.35)
        p.drawLine(x1, y1, x2, y2)
        tip = QtGui.QPolygon(
            [
                QtCore.QPoint(x2, y2),
                QtCore.QPoint(x2 - 6, y2 + 2),
                QtCore.QPoint(x2 - 2, y2 + 6),
            ]
        )
        p.drawPolygon(tip)

    def _drawPlayIcon(self, p: QtGui.QPainter, rect: QtCore.QRect):
        s = 30
        cx = rect.center().x()
        cy = rect.center().y()
        r = s // 2
        pen = QtGui.QPen(self._fgColor())
        pen.setWidth(max(1, 2))
        p.setPen(pen)
        p.setBrush(QtCore.Qt.NoBrush)
        p.drawEllipse(QtCore.QPoint(cx, cy), r, r)
        tri = QtGui.QPolygon(
            [
                QtCore.QPoint(cx - int(r * 0.3), cy - int(r * 0.45)),
                QtCore.QPoint(cx - int(r * 0.3), cy + int(r * 0.45)),
                QtCore.QPoint(cx + int(r * 0.5), cy),
            ]
        )
        p.setBrush(self._fgColor())
        p.drawPolygon(tri)


class EditModeToggle(QtWidgets.QWidget):
    selectionChanged = QtCore.pyqtSignal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._selected = 0
        self.setFixedSize(192, 32)
        self.setMouseTracking(True)
        Panel.applyDisabledOpacity(self)

    def sizeHint(self):
        return QtCore.QSize(192, 32)

    def setSelected(self, idx: int):
        if idx != self._selected:
            self._selected = idx
            self.update()

    def _drawTileIcon(self, p: QtGui.QPainter, rect: QtCore.QRect):
        s = 30
        cx = rect.center().x()
        cy = rect.center().y()
        x0 = cx - s // 2
        y0 = cy - s // 2
        pen = QtGui.QPen(QtGui.QColor(200, 200, 200))
        pen.setWidth(max(1, 2))
        p.setPen(pen)
        p.setBrush(QtGui.QBrush(QtGui.QColor(90, 90, 90)))
        cols = 3
        rows = 3
        cell = s // cols
        for j in range(rows):
            for i in range(cols):
                r = QtCore.QRect(x0 + i * cell + 2, y0 + j * cell + 2, cell - 4, cell - 4)
                p.drawRoundedRect(r, 3, 3)

    def _drawLightIcon(self, p: QtGui.QPainter, rect: QtCore.QRect):
        s = 30
        cx = rect.center().x()
        cy = rect.center().y()
        fg = QtGui.QColor(200, 200, 200)
        pen = QtGui.QPen(fg)
        pen.setWidth(max(1, 2))
        p.setPen(pen)
        p.setBrush(QtCore.Qt.NoBrush)

        bulb_r = 8
        bulb_center = QtCore.QPoint(cx, cy - 2)
        p.drawEllipse(bulb_center, bulb_r, bulb_r)

        base_w = 10
        base_h = 6
        base_rect = QtCore.QRect(cx - base_w // 2, cy + 8, base_w, base_h)
        p.drawRoundedRect(base_rect, 2, 2)
        p.drawLine(cx, cy + 6, cx, cy + 8)

        acc = QtWidgets.QApplication.palette().highlight().color()
        pen2 = QtGui.QPen(acc)
        pen2.setWidth(max(1, 2))
        p.setPen(pen2)
        ray = 12
        p.drawLine(cx, cy - 14, cx, cy - ray)
        p.drawLine(cx - 12, cy - 2, cx - ray, cy - 2)
        p.drawLine(cx + ray, cy - 2, cx + 12, cy - 2)
        p.drawLine(cx - 10, cy - 12, cx - 7, cy - 9)
        p.drawLine(cx + 7, cy - 9, cx + 10, cy - 12)

    def _drawActorIcon(self, p: QtGui.QPainter, rect: QtCore.QRect):
        s = 30
        cx = rect.center().x()
        cy = rect.center().y()
        fg = QtGui.QColor(200, 200, 200)
        pen = QtGui.QPen(fg)
        pen.setWidth(max(1, 2))
        p.setPen(pen)
        p.setBrush(QtGui.QBrush(fg))
        r_head = max(3, 6)
        p.drawEllipse(QtCore.QPoint(cx, cy - int(s * 0.22)), r_head, r_head)
        p.setBrush(QtCore.Qt.NoBrush)
        shoulders = QtGui.QPainterPath()
        bw = 18
        bh = 10
        x0 = cx - bw // 2
        y0 = cy + int(s * 0.02)
        shoulders.addRoundedRect(QtCore.QRectF(x0, y0, bw, bh), 4, 4)
        p.drawPath(shoulders)
        acc = QtWidgets.QApplication.palette().highlight().color()
        star = QtGui.QPolygon(
            [
                QtCore.QPoint(cx + int(s * 0.22), cy - int(s * 0.05)),
                QtCore.QPoint(cx + int(s * 0.16), cy + int(s * 0.02)),
                QtCore.QPoint(cx + int(s * 0.26), cy + int(s * 0.02)),
                QtCore.QPoint(cx + int(s * 0.18), cy + int(s * 0.08)),
                QtCore.QPoint(cx + int(s * 0.24), cy + int(s * 0.16)),
                QtCore.QPoint(cx + int(s * 0.14), cy + int(s * 0.12)),
                QtCore.QPoint(cx + int(s * 0.06), cy + int(s * 0.18)),
                QtCore.QPoint(cx + int(s * 0.08), cy + int(s * 0.08)),
                QtCore.QPoint(cx - int(s * 0.02), cy + int(s * 0.1)),
                QtCore.QPoint(cx + int(s * 0.06), cy + int(s * 0.02)),
            ]
        )
        p.setBrush(acc)
        p.drawPolygon(star)

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
        self._drawTileIcon(p, rectL)
        self._drawLightIcon(p, rectM)
        self._drawActorIcon(p, rectR)
        sel = rectL if self._selected == 0 else (rectM if self._selected == 1 else rectR)
        overlay = QtGui.QColor(255, 255, 255, 80)
        p.fillRect(sel, overlay)

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
            self.selectionChanged.emit(idx)

    def mouseMoveEvent(self, e):
        idx = self._idxFromX(e.x())
        if idx == 0:
            text = Locale.getContent("TILE_MODE")
        elif idx == 1:
            text = Locale.getContent("LIGHT_MODE")
        else:
            text = Locale.getContent("ACTOR_MODE")
        QtWidgets.QToolTip.showText(e.globalPos(), text, self)
        super().mouseMoveEvent(e)

    def event(self, e):
        if e.type() == QtCore.QEvent.ToolTip:
            idx = self._idxFromX(e.pos().x())
            if idx == 0:
                text = Locale.getContent("TILE_MODE")
            elif idx == 1:
                text = Locale.getContent("LIGHT_MODE")
            else:
                text = Locale.getContent("ACTOR_MODE")
            QtWidgets.QToolTip.showText(e.globalPos(), text, self)
            return True
        return super().event(e)

    def leaveEvent(self, e):
        QtWidgets.QToolTip.hideText()
        e.accept()

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Panel.applyDisabledOpacity(self)
        super().changeEvent(e)
