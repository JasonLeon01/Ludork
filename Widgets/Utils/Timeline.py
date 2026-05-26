# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
import wave
import contextlib
from PyQt5 import QtWidgets, QtGui, QtCore, QtMultimedia
from typing import Dict, Any, Optional, Tuple, List
from EditorGlobal import EditorStatus

FRAME_SEGMENT_DURATION = 0.05


class TimelineCanvas(QtWidgets.QWidget):
    DATA_CHANGED = QtCore.pyqtSignal()
    SELECTION_CHANGED = QtCore.pyqtSignal(int, int)
    TIME_CHANGED = QtCore.pyqtSignal(float)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
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
        self.waveformCache: Dict[str, Tuple[float, int, float, List[float]]] = {}
        self._tempPlayers: List[QtMultimedia.QMediaPlayer] = []

    def setData(self, data: Dict[str, Any]) -> None:
        self.data = data
        if self.data:
            self.frameRate = self.data.get("frameRate", 30)
        self.selectedSegment = None
        self.SELECTION_CHANGED.emit(-1, -1)
        self.updateCanvasSize()
        self.update()

    def setSelectedSegment(self, trackIdx: int, segIdx: int) -> None:
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
            self.SELECTION_CHANGED.emit(newSelected[0], newSelected[1])
        else:
            self.SELECTION_CHANGED.emit(-1, -1)
        self.update()

    def setZoom(self, zoom: float) -> None:
        self.zoom = zoom
        self.pixelsPerSecond = self.basePixelsPerSecond * self.zoom
        self.updateCanvasSize()
        self.update()

    def updateCanvasSize(self) -> None:
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

                isSound = seg.get("type") == "sound"

                painter.setPen(QtCore.Qt.NoPen)
                if isSound:
                    painter.setBrush(QtGui.QColor("#7a5a3c") if isSelected else QtGui.QColor("#5f4c38"))
                elif isSelected:
                    painter.setBrush(QtGui.QColor("#7bafe6"))
                else:
                    painter.setBrush(QtGui.QColor("#5a9fd6"))
                painter.drawRoundedRect(segRect, 4, 4)

                if isSound and 0 <= assetIdx < len(assets):
                    name = assets[assetIdx]
                    originalDuration = seg.get("originalDuration")
                    self._drawSoundWaveform(painter, segRect, name, max(0.0, end - start), originalDuration)

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

    def _isAudioAsset(self, assetName: str) -> bool:
        ext = os.path.splitext(assetName)[1].lower()
        return ext in [".wav", ".ogg", ".mp3"]

    def _drawSoundWaveform(
        self,
        painter: QtGui.QPainter,
        rect: QtCore.QRectF,
        assetName: str,
        duration: float,
        originalDuration: Optional[float],
    ):
        audioDuration, peaks = self._getWaveformData(assetName)
        if not peaks or audioDuration <= 0:
            painter.save()
            painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 90), 1))
            y = rect.center().y()
            painter.drawLine(QtCore.QPointF(rect.left() + 4, y), QtCore.QPointF(rect.right() - 4, y))
            painter.restore()
            return

        left = int(rect.left()) + 4
        right = int(rect.right()) - 4
        if right <= left:
            return

        fullDuration = audioDuration
        if isinstance(originalDuration, (int, float)) and originalDuration > 0:
            fullDuration = min(audioDuration, float(originalDuration))
        fullWidth = max(1.0, fullDuration * self.pixelsPerSecond)
        midY = rect.center().y()
        height = max(1.0, (rect.height() - 10.0) * 0.5)

        painter.save()
        painter.setClipRect(rect.adjusted(2, 2, -2, -2))
        painter.setPen(QtGui.QPen(QtGui.QColor(255, 236, 178, 180), 1))
        for x in range(left, right + 1):
            sourceTime = ((x - left) / fullWidth) * fullDuration
            if sourceTime > duration:
                break
            peakIdx = min(len(peaks) - 1, int((sourceTime / audioDuration) * len(peaks)))
            peak = peaks[peakIdx]
            painter.drawLine(QtCore.QPointF(x, midY - peak * height), QtCore.QPointF(x, midY + peak * height))
        painter.restore()

    def _getWaveformData(self, assetName: str) -> Tuple[float, List[float]]:
        assetPath = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Sounds", assetName)
        if not os.path.exists(assetPath):
            return 0.0, []
        stat = os.stat(assetPath)
        cache = self.waveformCache.get(assetPath)
        if cache and cache[0] == stat.st_mtime and cache[1] == stat.st_size:
            return cache[2], cache[3]
        duration, peaks = self._loadWaveformData(assetPath)
        self.waveformCache[assetPath] = (stat.st_mtime, stat.st_size, duration, peaks)
        return duration, peaks

    def _loadWaveformData(self, assetPath: str) -> Tuple[float, List[float]]:
        if os.path.splitext(assetPath)[1].lower() != ".wav":
            return 0.0, []
        try:
            with contextlib.closing(wave.open(assetPath, "rb")) as reader:
                frameCount = reader.getnframes()
                frameRate = reader.getframerate()
                sampleWidth = reader.getsampwidth()
                channelCount = reader.getnchannels()
                if frameCount <= 0 or frameRate <= 0 or sampleWidth not in (1, 2, 3, 4):
                    return 0.0, []
                duration = frameCount / float(frameRate)
                peakCount = 4096
                framesPerPeak = max(1, frameCount // peakCount)
                peaks = []
                framesRead = 0
                while framesRead < frameCount:
                    framesToRead = min(framesPerPeak, frameCount - framesRead)
                    data = reader.readframes(framesToRead)
                    if not data:
                        break
                    peaks.append(self._pcmPeak(data, sampleWidth, channelCount))
                    framesRead += framesToRead
                return duration, peaks
        except Exception as e:
            print(f"Error reading waveform: {e}")
            return 0.0, []

    def _pcmPeak(self, data: bytes, sampleWidth: int, channelCount: int) -> float:
        if not data:
            return 0.0
        if sampleWidth == 1:
            maxValue = 128.0
            return min(1.0, max(abs(b - 128) for b in data) / maxValue)

        maxValue = float(1 << (sampleWidth * 8 - 1))
        peak = 0
        step = max(1, sampleWidth)
        usableLength = len(data) - (len(data) % (step * max(1, channelCount)))
        for offset in range(0, usableLength, step):
            value = int.from_bytes(data[offset : offset + step], "little", signed=True)
            peak = max(peak, abs(value))
        return min(1.0, peak / maxValue)

    def _defaultDurationForAsset(self, assetName: str) -> float:
        if self._isAudioAsset(assetName):
            return 0.1
        return FRAME_SEGMENT_DURATION

    def _minSegmentDuration(self, seg: Dict[str, Any]) -> float:
        if seg.get("type") == "sound":
            return 1.0 / self.frameRate
        return FRAME_SEGMENT_DURATION

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
            actDelete = menu.addAction(ELOC("DELETE"))
            if actDelete is not None:
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
                        self.SELECTION_CHANGED.emit(-1, -1)
                        self.DATA_CHANGED.emit()
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
                self.SELECTION_CHANGED.emit(trackIdx, segIdx)
                self.dragMode = handle
                self.dragStartPos = pos

                seg = self.data["timeLines"][trackIdx]["timeSegments"][segIdx]
                self.dragOriginalStart = seg.get("startFrame", {}).get("time", 0.0)
                self.dragOriginalEnd = seg.get("endFrame", {}).get("time", 0.0)
                self.currentTime = (self.dragOriginalStart + self.dragOriginalEnd) * 0.5
                self.TIME_CHANGED.emit(self.currentTime)
            else:
                self.selectedSegment = None
                self.SELECTION_CHANGED.emit(-1, -1)
                x = event.x()
                time = x / self.pixelsPerSecond
                self.currentTime = max(0, time)
                self.TIME_CHANGED.emit(self.currentTime)

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
                maxStart = newEnd - self._minSegmentDuration(seg)

                minStartDur = newEnd - maxDur
                minStart = max(0.0, limitLeft, minStartDur)
                newStart = max(minStart, min(potentialStart, maxStart))
            elif self.dragMode == 3:  # Resize Right
                potentialEnd = self._snapTime(self.dragOriginalEnd + deltaTime)
                _, limitRight = self._findBounds(trackIdx, segIdx)
                minEnd = newStart + self._minSegmentDuration(seg)

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
            self.SELECTION_CHANGED.emit(trackIdx, segIdx)
        else:
            x = event.x()
            time = x / self.pixelsPerSecond
            self.currentTime = max(0, time)
            self.TIME_CHANGED.emit(self.currentTime)
            self.update()

    def mouseReleaseEvent(self, event):
        if self.dragMode != 0:
            self.dragMode = 0
            self.DATA_CHANGED.emit()

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
            assetName = event.mimeData().text()
            duration = self._defaultDurationForAsset(assetName)
            if self._checkOverlap(trackIdx, time, time + duration):
                event.ignore()
                return

        event.acceptProposedAction()

    def _getAudioDuration(self, filePath: str, segment: Dict[str, Any]) -> None:
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

        self._tempPlayers.append(player)

    def _onMediaError(self, player: QtMultimedia.QMediaPlayer) -> None:
        print(f"Media player error: {player.errorString()}")
        if player in self._tempPlayers:
            self._tempPlayers.remove(player)
        player.deleteLater()

    def _onMediaDurationChanged(
        self, player: QtMultimedia.QMediaPlayer, segment: Dict[str, Any], durationMs: int
    ) -> None:
        if durationMs > 0:
            duration = durationMs / 1000.0
            self._updateSegmentDuration(segment, duration)
            if player in self._tempPlayers:
                self._tempPlayers.remove(player)
            player.deleteLater()

    def _updateSegmentDuration(self, segment: Dict[str, Any], duration: float) -> None:
        start = segment["startFrame"]["time"]
        segment["endFrame"]["time"] = start + duration
        segment["originalDuration"] = duration
        self.updateCanvasSize()
        self.update()
        self.DATA_CHANGED.emit()

    def dropEvent(self, event: QtGui.QDropEvent) -> None:
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

        isAudio = self._isAudioAsset(assetName)
        duration = self._defaultDurationForAsset(assetName)

        if trackIdx >= 0 and self._checkOverlap(trackIdx, time, time + duration):
            return

        ext = os.path.splitext(assetName)[1].lower()
        assetPath = ""

        if isAudio:
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
        self.DATA_CHANGED.emit()


class TimelinePanel(QtWidgets.QWidget):
    TIME_CHANGED = QtCore.pyqtSignal(float)
    DATA_CHANGED = QtCore.pyqtSignal()
    SELECTION_CHANGED = QtCore.pyqtSignal(int, int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self.layout = QtWidgets.QVBoxLayout(self)
        self.layout.setContentsMargins(0, 0, 0, 0)
        self.layout.setSpacing(0)
        self._isPlaying = False
        self._playbackEndTime = 0.0
        self._playbackClock = QtCore.QElapsedTimer()
        self._playbackTimer = QtCore.QTimer(self)
        self._playbackTimer.setTimerType(QtCore.Qt.PreciseTimer)
        self._playbackTimer.timeout.connect(self._onPlaybackTimeout)
        self._soundPlayers: Dict[Tuple[int, int], QtMultimedia.QMediaPlayer] = {}

        self.toolbar = QtWidgets.QWidget()
        self.toolbar.setFixedHeight(28)
        self.toolbar.setStyleSheet("background-color: #333; border-bottom: 1px solid #222;")
        tbLayout = QtWidgets.QHBoxLayout(self.toolbar)
        tbLayout.setContentsMargins(8, 0, 8, 0)

        self.btnPlay = QtWidgets.QPushButton(ELOC("PLAY_ANIMATION"))
        self.btnPlay.setFixedHeight(22)
        self.btnPlay.clicked.connect(self._onPlayClicked)

        lblZoom = QtWidgets.QLabel("Zoom")
        lblZoom.setStyleSheet("color: #aaa;")
        self.sliderZoom = QtWidgets.QSlider(QtCore.Qt.Horizontal)
        self.sliderZoom.setRange(20, 500)
        self.sliderZoom.setValue(100)
        self.sliderZoom.setFixedWidth(120)
        self.sliderZoom.valueChanged.connect(self._onZoomChanged)

        tbLayout.addWidget(self.btnPlay)
        tbLayout.addSpacing(12)
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

        self.canvas.DATA_CHANGED.connect(self.DATA_CHANGED.emit)
        self.canvas.SELECTION_CHANGED.connect(self.SELECTION_CHANGED.emit)
        self.canvas.TIME_CHANGED.connect(self.TIME_CHANGED.emit)

        self.layout.addWidget(self.scrollArea)
        desiredHeight = self.toolbar.height() + self.canvas.headerHeight + self.canvas.trackHeight * 3 + 12
        self.setMinimumHeight(desiredHeight)

    def setData(self, data: Dict[str, Any]) -> None:
        self.stopPlayback()
        self.canvas.setData(data)

    def _onZoomChanged(self, value: int) -> None:
        self.canvas.setZoom(value / 100.0)

    def setSelectedSegment(self, trackIdx: int, segIdx: int) -> None:
        self.canvas.setSelectedSegment(trackIdx, segIdx)

    def _onPlayClicked(self) -> None:
        if self._isPlaying:
            self.stopPlayback()
            return
        self.playOnce()

    def playOnce(self) -> None:
        duration = self._getContentDuration()
        if duration <= 0:
            self._setCurrentTime(0.0)
            return

        self._isPlaying = True
        self._playbackEndTime = duration
        self.btnPlay.setText(ELOC("STOP_ANIMATION"))
        self._stopPlaybackSounds()
        self._setCurrentTime(0.0)
        self._playbackClock.restart()
        interval = max(1, int(1000 / max(1, self.canvas.frameRate) / 2))
        self._playbackTimer.start(interval)

    def stopPlayback(self) -> None:
        wasPlaying = self._isPlaying
        self._isPlaying = False
        self._playbackTimer.stop()
        self._stopPlaybackSounds()
        if wasPlaying:
            self.btnPlay.setText(ELOC("PLAY_ANIMATION"))

    def _onPlaybackTimeout(self) -> None:
        time = self._playbackClock.elapsed() / 1000.0
        if time >= self._playbackEndTime:
            self._setCurrentTime(self._playbackEndTime)
            self.stopPlayback()
            return
        self._setCurrentTime(time)

    def _setCurrentTime(self, time: float) -> None:
        self.canvas.currentTime = max(0.0, time)
        self.canvas.TIME_CHANGED.emit(self.canvas.currentTime)
        self.canvas.update()
        self._ensureTimeVisible(self.canvas.currentTime)
        if self._isPlaying:
            self._syncPlaybackSounds(self.canvas.currentTime)

    def _ensureTimeVisible(self, time: float) -> None:
        x = int(time * self.canvas.pixelsPerSecond)
        bar = self.scrollArea.horizontalScrollBar()
        if x < bar.value():
            bar.setValue(max(0, x - 24))
        elif x > bar.value() + self.scrollArea.viewport().width():
            bar.setValue(x - self.scrollArea.viewport().width() + 24)

    def _getContentDuration(self) -> float:
        duration = 0.0
        if not self.canvas.data:
            return duration
        for timeline in self.canvas.data.get("timeLines", []):
            for segment in timeline.get("timeSegments", []):
                duration = max(duration, segment.get("endFrame", {}).get("time", 0.0))
        return duration

    def _syncPlaybackSounds(self, time: float) -> None:
        activeKeys = set()
        if self.canvas.data:
            timeLines = self.canvas.data.get("timeLines", [])
            assets = self.canvas.data.get("assets", [])
            for trackIdx, timeline in enumerate(timeLines):
                for segIdx, segment in enumerate(timeline.get("timeSegments", [])):
                    if segment.get("type") != "sound":
                        continue
                    start = segment.get("startFrame", {}).get("time", 0.0)
                    end = segment.get("endFrame", {}).get("time", 0.0)
                    if not (start <= time < end):
                        continue
                    key = (trackIdx, segIdx)
                    activeKeys.add(key)
                    if key in self._soundPlayers:
                        continue
                    player = self._createSoundPlayer(segment, assets, time - start)
                    if player:
                        self._soundPlayers[key] = player
                        player.play()

        for key in list(self._soundPlayers.keys()):
            if key not in activeKeys:
                self._stopSoundPlayer(key)

    def _createSoundPlayer(
        self, segment: Dict[str, Any], assets: List[str], offset: float
    ) -> Optional[QtMultimedia.QMediaPlayer]:
        assetIdx = segment.get("asset", -1)
        if assetIdx < 0 or assetIdx >= len(assets):
            return None
        assetName = assets[assetIdx]
        assetPath = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Sounds", assetName)
        if not os.path.exists(assetPath):
            return None
        player = QtMultimedia.QMediaPlayer(self)
        player.setMedia(QtMultimedia.QMediaContent(QtCore.QUrl.fromLocalFile(assetPath)))
        player.setPosition(max(0, int(offset * 1000)))
        return player

    def _stopSoundPlayer(self, key: Tuple[int, int]) -> None:
        player = self._soundPlayers.pop(key, None)
        if not player:
            return
        player.stop()
        player.deleteLater()

    def _stopPlaybackSounds(self) -> None:
        for key in list(self._soundPlayers.keys()):
            self._stopSoundPlayer(key)
