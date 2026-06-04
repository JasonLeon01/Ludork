# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Optional
from PyQt5 import QtCore, QtGui, QtWidgets


class PerformanceMonitorWindow(QtWidgets.QDialog):
    CLOSED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.setWindowTitle(ELOC("PERFORMANCE_MONITOR"))
        self.resize(800, 400)
        self.setWindowFlags(self.windowFlags() & ~QtCore.Qt.WindowContextHelpButtonHint)
        self.setMouseTracking(True)
        self._fpsData: list[float] = []
        self._memoryData: list[float] = []
        self._timeData: list[float] = []
        self._hoverPos: Optional[QtCore.QPoint] = None

    def clearData(self) -> None:
        self._fpsData.clear()
        self._memoryData.clear()
        self._timeData.clear()
        self._hoverPos = None
        self.update()

    def addSample(self, fps: float, memoryMB: float) -> None:
        if fps <= 0:
            return
        currentTime = self._timeData[-1] if self._timeData else 0.0
        if self._fpsData:
            currentTime += 30.0 / max(self._fpsData[-1], 1.0)
        self._fpsData.append(float(fps))
        self._memoryData.append(max(0.0, float(memoryMB)))
        self._timeData.append(currentTime)
        self.update()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        self._hoverPos = event.pos()
        self.update()

    def leaveEvent(self, event: QtCore.QEvent) -> None:
        self._hoverPos = None
        self.update()

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:
        self.CLOSED.emit()
        super().closeEvent(event)

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.Antialiasing)
        painter.fillRect(self.rect(), QtGui.QColor(30, 30, 30))

        if not self._fpsData:
            self._drawWaitingText(painter)
            return

        maxFPS = max(self._fpsData)
        maxValue = max(65.0, maxFPS * 1.1)
        minValue = 0.0
        margin = 40
        width = self.width()
        height = self.height()
        graphWidth = max(1, width - margin * 2)
        graphHeight = max(1, height - margin * 2)
        count = len(self._fpsData)

        self._drawGrid(painter, margin, width, height, graphHeight, minValue, maxValue)
        if count >= 2:
            self._drawCurve(painter, margin, graphWidth, graphHeight, count, minValue, maxValue)
        self._drawStats(painter, margin, maxFPS, count)
        self._drawHoverInfo(painter, margin, width, height, graphWidth, graphHeight, count, minValue, maxValue)

    def _drawWaitingText(self, painter: QtGui.QPainter) -> None:
        painter.setPen(QtGui.QColor(180, 180, 180))
        painter.setFont(QtGui.QFont("Arial", 12))
        painter.drawText(self.rect(), QtCore.Qt.AlignCenter, ELOC("PERFORMANCE_MONITOR_WAITING"))

    def _drawGrid(
        self,
        painter: QtGui.QPainter,
        margin: int,
        width: int,
        height: int,
        graphHeight: int,
        minValue: float,
        maxValue: float,
    ) -> None:
        scaleY = graphHeight / (maxValue - minValue)
        painter.setFont(QtGui.QFont("Arial", 8))
        for fpsMark in [30, 60, 90, 120, 150]:
            y = margin + graphHeight - (fpsMark - minValue) * scaleY
            if margin <= y <= height - margin:
                painter.setPen(QtGui.QPen(QtGui.QColor(100, 100, 100), 1, QtCore.Qt.DashLine))
                painter.drawLine(margin, int(y), width - margin, int(y))
                painter.setPen(QtGui.QColor(200, 200, 200))
                painter.drawText(5, int(y) + 4, f"{fpsMark}")

    def _drawCurve(
        self,
        painter: QtGui.QPainter,
        margin: int,
        graphWidth: int,
        graphHeight: int,
        count: int,
        minValue: float,
        maxValue: float,
    ) -> None:
        stepX = graphWidth / (count - 1)
        scaleY = graphHeight / (maxValue - minValue)
        painter.setPen(QtGui.QPen(QtGui.QColor(0, 255, 0), 1))
        path = QtGui.QPainterPath()
        startY = margin + graphHeight - (self._fpsData[0] - minValue) * scaleY
        path.moveTo(margin, startY)
        for i in range(1, count):
            x = margin + i * stepX
            y = margin + graphHeight - (self._fpsData[i] - minValue) * scaleY
            path.lineTo(x, y)
        painter.drawPath(path)

    def _drawStats(self, painter: QtGui.QPainter, margin: int, maxFPS: float, count: int) -> None:
        minFPS = min(self._fpsData)
        avgFPS = sum(self._fpsData) / count
        memoryMB = self._memoryData[-1] if self._memoryData else 0.0
        painter.setPen(QtGui.QColor(255, 255, 255))
        painter.setFont(QtGui.QFont("Arial", 10))
        stats = ELOC("PERFORMANCE_MONITOR_STATS").format(
            avg=avgFPS,
            max=maxFPS,
            min=minFPS,
            memory=memoryMB,
            count=count,
        )
        painter.drawText(margin, margin - 10, stats)

    def _drawHoverInfo(
        self,
        painter: QtGui.QPainter,
        margin: int,
        width: int,
        height: int,
        graphWidth: int,
        graphHeight: int,
        count: int,
        minValue: float,
        maxValue: float,
    ) -> None:
        if self._hoverPos is None or count < 2:
            return
        mx = self._hoverPos.x()
        if not margin <= mx <= width - margin:
            return
        stepX = graphWidth / (count - 1)
        scaleY = graphHeight / (maxValue - minValue)
        idx = int((mx - margin) / stepX)
        if not 0 <= idx < count:
            return
        fps = self._fpsData[idx]
        memoryMB = self._memoryData[idx] if idx < len(self._memoryData) else 0.0
        timeSec = self._timeData[idx]
        x = margin + idx * stepX
        y = margin + graphHeight - (fps - minValue) * scaleY

        painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 0), 1, QtCore.Qt.DashLine))
        painter.drawLine(int(x), margin, int(x), height - margin)
        painter.drawLine(margin, int(y), width - margin, int(y))

        infoText = ELOC("PERFORMANCE_MONITOR_POINT").format(time=timeSec, fps=fps, memory=memoryMB)
        fm = painter.fontMetrics()
        textWidth = fm.width(infoText) + 10
        textHeight = fm.height() + 10
        textX = int(x) + 10
        textY = int(y) - 10
        if textX + textWidth > width - margin:
            textX = int(x) - textWidth - 10
        if textY - textHeight < margin:
            textY = int(y) + textHeight + 10

        painter.setBrush(QtGui.QColor(0, 0, 0, 180))
        painter.setPen(QtCore.Qt.NoPen)
        painter.drawRoundedRect(textX, textY - textHeight, textWidth, textHeight, 5, 5)
        painter.setPen(QtGui.QColor(255, 255, 255))
        painter.drawText(textX + 5, textY - 5, infoText)
