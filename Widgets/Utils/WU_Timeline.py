# -*- encoding: utf-8 -*-

import os
import wave
import contextlib
from PyQt5 import QtWidgets, QtGui, QtCore, QtMultimedia
from typing import Dict, Any, Optional, Tuple
from Utils import Locale
import EditorStatus


class TimelineCanvas(QtWidgets.QWidget):
    dataChanged = QtCore.pyqtSignal()
    selectionChanged = QtCore.pyqtSignal(int, int)
    timeChanged = QtCore.pyqtSignal(float)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.data: Dict[str, Any] = {}
        self.frameRate = 30
        self.basePixelsPerSecond = 300
        self.pixelsPerSecond = 300
        self.headerHeight = 30
        self.trackHeight = 40
        self.currentTime = 0.0
        self.setMouseTracking(True)
        self.setAcceptDrops(True)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.zoom = 1.0

        self.selectedSegment: Optional[Tuple[int, int]] = None
        self.dragMode = 0  # 0: None, 1: Move, 2: ResizeLeft, 3: ResizeRight
        self.dragStartPos = QtCore.QPoint()
        self.dragOriginalStart = 0.0
        self.dragOriginalEnd = 0.0

    def setData(self, data: Dict[str, Any]):
        self.data = data
        if self.data:
            self.frameRate = self.data.get("frameRate", 30)
        self.selectedSegment = None
        self.selectionChanged.emit(-1, -1)
        self.updateCanvasSize()
        self.update()

    def setSelectedSegment(self, trackIdx: int, segIdx: int):
        newSelected = None
        if self.data and trackIdx >= 0 and segIdx >= 0:
            timeLines = self.data.get("timeLines", [])
            if trackIdx < len(timeLines):
                segments = timeLines[trackIdx].get("timeSegments", [])
                if segIdx < len(segments):
                    newSelected = (trackIdx, segIdx)

        if self.selectedSegment == newSelected:
            return

        self.selectedSegment = newSelected
        if newSelected:
            self.selectionChanged.emit(newSelected[0], newSelected[1])
        else:
            self.selectionChanged.emit(-1, -1)
        self.update()

    def setZoom(self, zoom: float):
        self.zoom = zoom
        self.pixelsPerSecond = self.basePixelsPerSecond * self.zoom
        self.updateCanvasSize()
        self.update()

    def updateCanvasSize(self):
        maxTime = 5.0
        if self.data:
            timelines = self.data.get("timeLines", [])
            for tl in timelines:
                for seg in tl.get("timeSegments", []):
                    endTime = seg.get("endFrame", {}).get("time", 0.0)
                    if endTime > maxTime:
                        maxTime = endTime
        maxTime += 1.0
        width = int(maxTime * self.pixelsPerSecond)
        trackCount = 0
        if self.data:
            trackCount = len(self.data.get("timeLines", []))
        trackCount = max(trackCount, 5)
        height = self.headerHeight + (trackCount + 1) * self.trackHeight

        self.setMinimumSize(width, height)
        self.resize(width, height)

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)

        rect = self.rect()
        painter.fillRect(rect, QtGui.QColor("#2b2b2b"))

        trackCount = 0
        if self.data:
            trackCount = len(self.data.get("timeLines", []))

        displayTracks = max(trackCount, int((rect.height() - self.headerHeight) / self.trackHeight) + 1)

        for i in range(displayTracks):
            y = self.headerHeight + i * self.trackHeight
            trackRect = QtCore.QRect(0, y, rect.width(), self.trackHeight)
            if i % 2 == 0:
                painter.fillRect(trackRect, QtGui.QColor("#303030"))
            else:
                painter.fillRect(trackRect, QtGui.QColor("#2d2d2d"))

            painter.setPen(QtGui.QColor("#222222"))
            painter.drawLine(0, y + self.trackHeight, rect.width(), y + self.trackHeight)

        headerRect = QtCore.QRect(0, 0, rect.width(), self.headerHeight)
        painter.fillRect(headerRect, QtGui.QColor("#383838"))
        painter.setPen(QtGui.QColor("#888888"))
        painter.drawLine(0, self.headerHeight, rect.width(), self.headerHeight)

        startSec = 0
        endSec = rect.width() / self.pixelsPerSecond

        painter.setFont(QtGui.QFont("Segoe UI", 8))

        for s in range(int(startSec), int(endSec) + 1):
            x = int(s * self.pixelsPerSecond)
            painter.setPen(QtGui.QColor("#aaaaaa"))
            painter.drawLine(x, 0, x, self.headerHeight)
            painter.drawText(x + 4, self.headerHeight - 4, f"{s}s")

            if self.pixelsPerSecond > 50:
                step = 1.0 / self.frameRate
                frameStep = 1
                if self.pixelsPerSecond < 150:
                    frameStep = 5
                if self.pixelsPerSecond < 80:
                    frameStep = 10

                frame = 1
                while frame < self.frameRate:
                    frameT = s + frame * step
                    frameX = int(frameT * self.pixelsPerSecond)
                    if frameX > rect.width():
                        break

                    if frame % frameStep == 0:
                        h = self.headerHeight * 0.25
                        if frame % (self.frameRate // 2) == 0:
                            h = self.headerHeight * 0.5

                        painter.drawLine(frameX, int(self.headerHeight - h), frameX, self.headerHeight)

                    frame += 1

        if not self.data:
            return

        timeLines = self.data.get("timeLines", [])
        assets = self.data.get("assets", [])

        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)

        for i, timeline in enumerate(timeLines):
            y = self.headerHeight + i * self.trackHeight
            segments = timeline.get("timeSegments", [])
            for j, seg in enumerate(segments):
                start = seg.get("startFrame", {}).get("time", 0.0)
                end = seg.get("endFrame", {}).get("time", 0.0)
                assetIdx = seg.get("asset", -1)

                x1 = start * self.pixelsPerSecond
                x2 = end * self.pixelsPerSecond
                w = max(2, x2 - x1)
                segRect = QtCore.QRectF(x1, y + 4, w, self.trackHeight - 8)

                isSelected = self.selectedSegment == (i, j)

                painter.setPen(QtCore.Qt.NoPen)
                if isSelected:
                    painter.setBrush(QtGui.QColor("#7bafe6"))
                else:
                    painter.setBrush(QtGui.QColor("#5a9fd6"))
                painter.drawRoundedRect(segRect, 4, 4)

                painter.setBrush(QtCore.Qt.NoBrush)
                if isSelected:
                    painter.setPen(QtGui.QPen(QtGui.QColor("#ffffff"), 2))
                else:
                    painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 100), 1))
                painter.drawRoundedRect(segRect, 4, 4)

                tolerance = 0.01
                isHead = isSelected and abs(self.currentTime - start) <= tolerance
                isTail = isSelected and abs(self.currentTime - end) <= tolerance
                if isHead or isTail:
                    painter.setBrush(QtGui.QColor("#ffffff"))
                    painter.setPen(QtCore.Qt.NoPen)
                    top = segRect.top()
                    bottom = segRect.bottom()
                    mid = (top + bottom) * 0.5
                    marker = 6.0
                    if isHead:
                        leftX = segRect.left()
                        headPoly = QtGui.QPolygonF(
                            [
                                QtCore.QPointF(leftX, top),
                                QtCore.QPointF(leftX, bottom),
                                QtCore.QPointF(leftX + marker, mid),
                            ]
                        )
                        painter.drawPolygon(headPoly)
                    if isTail:
                        rightX = segRect.right()
                        tailPoly = QtGui.QPolygonF(
                            [
                                QtCore.QPointF(rightX, top),
                                QtCore.QPointF(rightX, bottom),
                                QtCore.QPointF(rightX - marker, mid),
                            ]
                        )
                        painter.drawPolygon(tailPoly)

                if 0 <= assetIdx < len(assets):
                    name = assets[assetIdx]
                    painter.setPen(QtGui.QColor("#ffffff"))
                    painter.drawText(segRect, QtCore.Qt.AlignCenter, name)

        painter.setRenderHint(QtGui.QPainter.Antialiasing, False)
        playheadX = int(self.currentTime * self.pixelsPerSecond)
        painter.setPen(QtGui.QPen(QtGui.QColor("#ff4444"), 1))
        painter.drawLine(playheadX, 0, playheadX, rect.height())

        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)
        painter.setBrush(QtGui.QColor("#ff4444"))
        painter.setPen(QtCore.Qt.NoPen)
        handle = [
            QtCore.QPoint(playheadX, 0),
            QtCore.QPoint(playheadX - 6, 0),
            QtCore.QPoint(playheadX - 6, 12),
            QtCore.QPoint(playheadX, 18),
            QtCore.QPoint(playheadX + 6, 12),
            QtCore.QPoint(playheadX + 6, 0),
        ]
        painter.drawPolygon(QtGui.QPolygon(handle))

    def _getTrackAt(self, y: int) -> int:
        if y < self.headerHeight:
            return -1
        return int((y - self.headerHeight) / self.trackHeight)

    def _getTimeAt(self, x: int) -> float:
        return x / self.pixelsPerSecond

    def _snapTime(self, time: float) -> float:
        step = 1.0 / self.frameRate
        return round(time / step) * step

    def _hitTest(self, pos: QtCore.QPoint) -> Tuple[int, int, int]:
        """Returns (trackIdx, segIdx, handle)
        handle: 0=None, 1=Body, 2=Left, 3=Right
        """
        trackIdx = self._getTrackAt(pos.y())
        if trackIdx < 0:
            return -1, -1, 0

        if not self.data:
            return -1, -1, 0

        timeLines = self.data.get("timeLines", [])
        if trackIdx >= len(timeLines):
            return -1, -1, 0

        segments = timeLines[trackIdx].get("timeSegments", [])
        time = self._getTimeAt(pos.x())

        handleWidth = 5.0 / self.pixelsPerSecond

        for i, seg in enumerate(segments):
            start = seg.get("startFrame", {}).get("time", 0.0)
            end = seg.get("endFrame", {}).get("time", 0.0)

            if start - handleWidth <= time <= end + handleWidth:
                if abs(time - start) <= handleWidth:
                    return trackIdx, i, 2  # Left
                elif abs(time - end) <= handleWidth:
                    return trackIdx, i, 3  # Right
                elif start < time < end:
                    return trackIdx, i, 1  # Body

        return -1, -1, 0

    def _findBounds(self, trackIdx: int, ignoreSegIdx: int) -> Tuple[float, float]:
        if not self.data:
            return 0.0, float("inf")
        timeLines = self.data.get("timeLines", [])
        if trackIdx >= len(timeLines):
            return 0.0, float("inf")
        segments = timeLines[trackIdx].get("timeSegments", [])

        limitLeft = 0.0
        limitRight = float("inf")
        origStart = self.dragOriginalStart
        origEnd = self.dragOriginalEnd

        for i, seg in enumerate(segments):
            if i == ignoreSegIdx:
                continue

            s = seg.get("startFrame", {}).get("time", 0.0)
            e = seg.get("endFrame", {}).get("time", 0.0)
            if e <= origStart + 0.0001:
                limitLeft = max(limitLeft, e)
            if s >= origEnd - 0.0001:
                limitRight = min(limitRight, s)

        return limitLeft, limitRight

    def _checkOverlap(self, trackIdx: int, start: float, end: float, ignoreSegIdx: int = -1) -> bool:
        if not self.data:
            return False
        timeLines = self.data.get("timeLines", [])
        if trackIdx >= len(timeLines):
            return False

        segments = timeLines[trackIdx].get("timeSegments", [])
        for i, seg in enumerate(segments):
            if i == ignoreSegIdx:
                continue
            s = seg.get("startFrame", {}).get("time", 0.0)
            e = seg.get("endFrame", {}).get("time", 0.0)
            if start < e and end > s:
                return True
        return False

    def contextMenuEvent(self, event):
        pos = event.pos()
        trackIdx, segIdx, handle = self._hitTest(pos)
        if trackIdx != -1 and handle != 0:
            self.selectedSegment = (trackIdx, segIdx)
            self.update()

            menu = QtWidgets.QMenu(self)
            actDelete = menu.addAction(Locale.getContent("DELETE"))
            actDelete.triggered.connect(self.deleteSelectedSegment)
            menu.exec_(event.globalPos())

    def deleteSelectedSegment(self):
        if self.selectedSegment:
            trackIdx, segIdx = self.selectedSegment
            if self.data and "timeLines" in self.data:
                timeLines = self.data["timeLines"]
                if 0 <= trackIdx < len(timeLines):
                    segments = timeLines[trackIdx].get("timeSegments", [])
                    if 0 <= segIdx < len(segments):
                        segments.pop(segIdx)
                        self.selectedSegment = None
                        self.selectionChanged.emit(-1, -1)
                        self.dataChanged.emit()
                        self.updateCanvasSize()
                        self.update()

    def keyPressEvent(self, event):
        if event.key() == QtCore.Qt.Key_Delete or event.key() == QtCore.Qt.Key_Backspace:
            self.deleteSelectedSegment()

    def mousePressEvent(self, event):
        if event.button() == QtCore.Qt.LeftButton:
            pos = event.pos()
            trackIdx, segIdx, handle = self._hitTest(pos)

            if trackIdx != -1 and handle != 0:
                self.selectedSegment = (trackIdx, segIdx)
                self.selectionChanged.emit(trackIdx, segIdx)
                self.dragMode = handle
                self.dragStartPos = pos

                seg = self.data["timeLines"][trackIdx]["timeSegments"][segIdx]
                self.dragOriginalStart = seg.get("startFrame", {}).get("time", 0.0)
                self.dragOriginalEnd = seg.get("endFrame", {}).get("time", 0.0)
                self.currentTime = (self.dragOriginalStart + self.dragOriginalEnd) * 0.5
                self.timeChanged.emit(self.currentTime)
            else:
                self.selectedSegment = None
                self.selectionChanged.emit(-1, -1)
                x = event.x()
                time = x / self.pixelsPerSecond
                self.currentTime = max(0, time)
                self.timeChanged.emit(self.currentTime)

            self.setFocus()
            self.update()

    def mouseMoveEvent(self, event):
        pos = event.pos()

        if not event.buttons() & QtCore.Qt.LeftButton:
            trackIdx, segIdx, handle = self._hitTest(pos)
            if handle == 2 or handle == 3:
                self.setCursor(QtCore.Qt.SizeHorCursor)
            elif handle == 1:
                self.setCursor(QtCore.Qt.SizeAllCursor)
            else:
                self.setCursor(QtCore.Qt.ArrowCursor)
            return

        if self.selectedSegment and self.dragMode != 0:
            trackIdx, segIdx = self.selectedSegment
            timeLines = self.data.get("timeLines", [])
            if trackIdx >= len(timeLines):
                return
            segments = timeLines[trackIdx].get("timeSegments", [])
            if segIdx >= len(segments):
                return

            seg = segments[segIdx]

            deltaX = pos.x() - self.dragStartPos.x()
            deltaTime = deltaX / self.pixelsPerSecond

            newStart = self.dragOriginalStart
            newEnd = self.dragOriginalEnd

            maxDur = seg.get("originalDuration", float("inf")) if seg.get("type") == "sound" else float("inf")

            if self.dragMode == 1:  # Move
                duration = newEnd - newStart
                potentialStart = self._snapTime(self.dragOriginalStart + deltaTime)
                potentialStart = max(0.0, potentialStart)
                limitLeft, limitRight = self._findBounds(trackIdx, segIdx)
                maxStart = limitRight - duration
                minStart = max(0.0, limitLeft)
                clampedStart = max(minStart, min(potentialStart, maxStart))
                newStart = clampedStart
                newEnd = newStart + duration
            elif self.dragMode == 2:  # Resize Left
                potentialStart = self._snapTime(self.dragOriginalStart + deltaTime)
                limitLeft, _ = self._findBounds(trackIdx, segIdx)
                maxStart = newEnd - (1.0 / self.frameRate)

                minStartDur = newEnd - maxDur
                minStart = max(0.0, limitLeft, minStartDur)
                newStart = max(minStart, min(potentialStart, maxStart))
            elif self.dragMode == 3:  # Resize Right
                potentialEnd = self._snapTime(self.dragOriginalEnd + deltaTime)
                _, limitRight = self._findBounds(trackIdx, segIdx)
                minEnd = newStart + (1.0 / self.frameRate)

                maxEndDur = newStart + maxDur
                maxEnd = min(limitRight, maxEndDur)
                newEnd = max(minEnd, min(potentialEnd, maxEnd))

            if "startFrame" not in seg:
                seg["startFrame"] = {}
            if "endFrame" not in seg:
                seg["endFrame"] = {}

            seg["startFrame"]["time"] = newStart
            seg["endFrame"]["time"] = newEnd

            self.updateCanvasSize()
            self.update()
            self.selectionChanged.emit(trackIdx, segIdx)
        else:
            x = event.x()
            time = x / self.pixelsPerSecond
            self.currentTime = max(0, time)
            self.timeChanged.emit(self.currentTime)
            self.update()

    def mouseReleaseEvent(self, event):
        if self.dragMode != 0:
            self.dragMode = 0
            self.dataChanged.emit()

    def dragEnterEvent(self, event):
        if event.mimeData().hasText():
            event.acceptProposedAction()

    def dragMoveEvent(self, event):
        pos = event.pos()
        trackIdx = self._getTrackAt(pos.y())
        if trackIdx >= 0:
            time = self._getTimeAt(pos.x())
            time = self._snapTime(time)
            if time < 0:
                time = 0.0
            if self._checkOverlap(trackIdx, time, time + 0.1):
                event.ignore()
                return

        event.acceptProposedAction()

    def _getAudioDuration(self, filePath: str, segment: Dict):
        if not os.path.exists(filePath):
            print(f"Audio file not found: {filePath}")
            return

        try:
            import importlib

            engine_mod = importlib.import_module("Engine")
            SoundBuffer = engine_mod.SoundBuffer

            buffer = SoundBuffer()
            if buffer.loadFromFile(filePath):
                duration = buffer.getDuration().asSeconds()
                self._updateSegmentDuration(segment, duration)
                return
        except Exception as e:
            print(f"SFML SoundBuffer load failed: {e}")

        ext = os.path.splitext(filePath)[1].lower()
        if ext == ".wav":
            try:
                with contextlib.closing(wave.open(filePath, "r")) as f:
                    frames = f.getnframes()
                    rate = f.getframerate()
                    duration = frames / float(rate)
                    self._updateSegmentDuration(segment, duration)
                    return
            except Exception as e:
                print(f"Error reading wav duration: {e}")

        player = QtMultimedia.QMediaPlayer(self)
        player.setMedia(QtMultimedia.QMediaContent(QtCore.QUrl.fromLocalFile(filePath)))
        player.durationChanged.connect(lambda d: self._onMediaDurationChanged(player, segment, d))
        player.error.connect(lambda: self._onMediaError(player))

        if not hasattr(self, "_tempPlayers"):
            self._tempPlayers = []
        self._tempPlayers.append(player)

    def _onMediaError(self, player):
        print(f"Media player error: {player.errorString()}")
        if hasattr(self, "_tempPlayers") and player in self._tempPlayers:
            self._tempPlayers.remove(player)
        player.deleteLater()

    def _onMediaDurationChanged(self, player, segment, durationMs):
        if durationMs > 0:
            duration = durationMs / 1000.0
            self._updateSegmentDuration(segment, duration)
            if hasattr(self, "_tempPlayers") and player in self._tempPlayers:
                self._tempPlayers.remove(player)
            player.deleteLater()

    def _updateSegmentDuration(self, segment, duration):
        start = segment["startFrame"]["time"]
        segment["endFrame"]["time"] = start + duration
        segment["originalDuration"] = duration
        self.updateCanvasSize()
        self.update()
        self.dataChanged.emit()

    def dropEvent(self, event):
        assetName = event.mimeData().text()
        assets = self.data.get("assets", [])

        try:
            assetIdx = assets.index(assetName)
        except ValueError:
            return

        pos = event.pos()
        time = self._getTimeAt(pos.x())
        trackIdx = self._getTrackAt(pos.y())

        time = self._snapTime(time)
        if time < 0:
            time = 0.0

        if trackIdx >= 0 and self._checkOverlap(trackIdx, time, time + 0.1):
            return

        isAudio = False
        duration = 0.1
        ext = os.path.splitext(assetName)[1].lower()
        assetPath = ""

        if ext in [".wav", ".ogg", ".mp3"]:
            isAudio = True
            assetPath = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Sounds", assetName)
        else:
            assetPath = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Animations", assetName)

        newSeg = {
            "type": "sound" if isAudio else "frame",
            "asset": assetIdx,
            "startFrame": {"time": time, "position": [0.0, 0.0], "rotation": 0.0, "scale": [1.0, 1.0]},
            "endFrame": {"time": time + duration, "position": [0.0, 0.0], "rotation": 0.0, "scale": [1.0, 1.0]},
        }

        if isAudio:
            newSeg["originalDuration"] = duration
            self._getAudioDuration(assetPath, newSeg)

            timeLines = self.data.get("timeLines", [])
            if 0 <= trackIdx < len(timeLines):
                startT = newSeg["startFrame"]["time"]
                endT = newSeg["endFrame"]["time"]
                limit = float("inf")

                for s in timeLines[trackIdx].get("timeSegments", []):
                    sStart = s["startFrame"]["time"]
                    if sStart >= startT and sStart < limit:
                        limit = sStart

                if endT > limit:
                    newSeg["endFrame"]["time"] = limit

        timeLines = self.data.setdefault("timeLines", [])

        if trackIdx < 0:
            trackIdx = 0

        if trackIdx >= len(timeLines):
            while len(timeLines) <= trackIdx:
                timeLines.append({"timeSegments": []})
            timeLines[trackIdx]["timeSegments"].append(newSeg)
        else:
            timeLines[trackIdx].setdefault("timeSegments", []).append(newSeg)

        self.updateCanvasSize()
        self.update()
        self.dataChanged.emit()


class TimelinePanel(QtWidgets.QWidget):
    timeChanged = QtCore.pyqtSignal(float)
    dataChanged = QtCore.pyqtSignal()
    selectionChanged = QtCore.pyqtSignal(int, int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.layout = QtWidgets.QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        self.layout.setSpacing(0)

        self.toolbar = QtWidgets.QWidget()
        self.toolbar.setFixedHeight(28)
        self.toolbar.setStyleSheet("background-color: #333; border-bottom: 1px solid #222;")
        tbLayout = QtWidgets.QHBoxLayout(self.toolbar)
        tbLayout.setContentsMargins(8, 0, 8, 0)

        lblZoom = QtWidgets.QLabel("Zoom")
        lblZoom.setStyleSheet("color: #aaa;")
        self.sliderZoom = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.sliderZoom.setRange(20, 500)
        self.sliderZoom.setValue(100)
        self.sliderZoom.setFixedWidth(120)
        self.sliderZoom.valueChanged.connect(self._onZoomChanged)

        tbLayout.addWidget(lblZoom)
        tbLayout.addWidget(self.sliderZoom)
        tbLayout.addStretch(1)

        self.layout.addWidget(self.toolbar)

        self.scrollArea = QtWidgets.QScrollArea()
        self.scrollArea.setWidgetResizable(True)
        self.scrollArea.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOn)
        self.scrollArea.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOn)
        self.scrollArea.setStyleSheet("QScrollArea { border: none; }")

        self.canvas = TimelineCanvas()
        self.scrollArea.setWidget(self.canvas)

        self.canvas.dataChanged.connect(self.dataChanged)
        self.canvas.selectionChanged.connect(self.selectionChanged)
        self.canvas.timeChanged.connect(self.timeChanged)

        self.layout.addWidget(self.scrollArea)
        desiredHeight = self.toolbar.height() + self.canvas.headerHeight + self.canvas.trackHeight * 3 + 12
        self.setMinimumHeight(desiredHeight)

    def setData(self, data):
        self.canvas.setData(data)

    def _onZoomChanged(self, value):
        self.canvas.setZoom(value / 100.0)

    def setSelectedSegment(self, trackIdx: int, segIdx: int):
        self.canvas.setSelectedSegment(trackIdx, segIdx)
