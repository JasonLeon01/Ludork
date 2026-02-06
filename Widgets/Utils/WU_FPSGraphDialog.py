# -*- encoding: utf-8 -*-

from PyQt5 import QtWidgets, QtGui, QtCore
from Utils import Locale


class FPSGraphDialog(QtWidgets.QDialog):
    def __init__(self, fpsData: list, parent=None):
        super().__init__(parent)
        self.setWindowTitle(Locale.getContent("FPS_HISTORY"))
        self.resize(800, 400)
        self.fpsData = fpsData
        self.setWindowFlags(self.windowFlags() & ~QtCore.Qt.WindowContextHelpButtonHint)
        self.setMouseTracking(True)
        self.hover_pos = None
        self.time_data = []
        current_time = 0.0
        for fps in self.fpsData:
            self.time_data.append(current_time)
            if fps > 0:
                current_time += 30.0 / fps

    def mouseMoveEvent(self, event):
        self.hover_pos = event.pos()
        self.update()

    def leaveEvent(self, event):
        self.hover_pos = None
        self.update()

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.Antialiasing)

        # Draw background
        painter.fillRect(self.rect(), QtGui.QColor(30, 30, 30))

        if not self.fpsData:
            return

        # Calculate scale
        maxFPS = max(self.fpsData)
        minFPS = min(self.fpsData)
        max_val = max(65, maxFPS * 1.1)
        min_val = 0

        w = self.width()
        h = self.height()
        margin = 40
        graph_w = w - margin * 2
        graph_h = h - margin * 2

        count = len(self.fpsData)
        if count < 2:
            return

        stepX = graph_w / (count - 1)
        scaleY = graph_h / (max_val - min_val)

        # Draw grid lines (e.g., 30, 60 FPS)
        painter.setFont(QtGui.QFont("Arial", 8))
        for fps_mark in [30, 60, 90, 120, 150]:
            y = margin + graph_h - (fps_mark - min_val) * scaleY
            if margin <= y <= h - margin:
                painter.setPen(QtGui.QPen(QtGui.QColor(100, 100, 100), 1, QtCore.Qt.DashLine))
                painter.drawLine(margin, int(y), w - margin, int(y))
                painter.setPen(QtGui.QColor(200, 200, 200))
                painter.drawText(5, int(y) + 4, f"{fps_mark}")

        # Draw curve
        painter.setPen(QtGui.QPen(QtGui.QColor(0, 255, 0), 1))
        path = QtGui.QPainterPath()

        startY = margin + graph_h - (self.fpsData[0] - min_val) * scaleY
        path.moveTo(margin, startY)

        # Optimize for drawing many points
        step_idx = max(1, count // graph_w)  # Skip points if more than pixels width

        for i in range(1, count):
            x = margin + i * stepX
            y = margin + graph_h - (self.fpsData[i] - min_val) * scaleY
            path.lineTo(x, y)

        painter.drawPath(path)

        # Draw stats
        avgFPS = sum(self.fpsData) / count
        painter.setPen(QtGui.QColor(255, 255, 255))
        painter.setFont(QtGui.QFont("Arial", 10))
        stats = f"Average: {avgFPS:.2f} | Max: {maxFPS:.2f} | Min: {minFPS:.2f} | Frames: {count}"
        painter.drawText(margin, margin - 10, stats)

        # Draw hover info
        if self.hover_pos:
            mx = self.hover_pos.x()
            if margin <= mx <= w - margin:
                idx = int((mx - margin) / stepX)
                if 0 <= idx < count:
                    fps = self.fpsData[idx]
                    time_sec = self.time_data[idx]

                    x = margin + idx * stepX
                    y = margin + graph_h - (fps - min_val) * scaleY

                    # Draw crosshair
                    painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 0), 1, QtCore.Qt.DashLine))
                    painter.drawLine(int(x), margin, int(x), h - margin)
                    painter.drawLine(margin, int(y), w - margin, int(y))

                    # Draw text bubble
                    info_text = f"Time: {time_sec:.1f}s | FPS: {fps:.1f}"
                    fm = painter.fontMetrics()
                    tw = fm.width(info_text) + 10
                    th = fm.height() + 10

                    tx = int(x) + 10
                    ty = int(y) - 10

                    if tx + tw > w - margin:
                        tx = int(x) - tw - 10
                    if ty - th < margin:
                        ty = int(y) + th + 10

                    painter.setBrush(QtGui.QColor(0, 0, 0, 180))
                    painter.setPen(QtCore.Qt.NoPen)
                    painter.drawRoundedRect(tx, ty - th, tw, th, 5, 5)

                    painter.setPen(QtGui.QColor(255, 255, 255))
                    painter.drawText(tx + 5, ty - 5, info_text)
