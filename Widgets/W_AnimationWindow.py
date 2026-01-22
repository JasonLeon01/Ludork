# -*- encoding: utf-8 -*-

import copy
import os
import math
from PyQt5 import QtCore, QtWidgets, QtGui, QtMultimedia
from typing import Optional, Dict, Any, List
from Utils import Locale
import EditorStatus
from Data import GameData
from .Utils import TimeLine


class AssetLabel(QtWidgets.QLabel):
    def __init__(self, assetName: str, parent=None):
        super().__init__(parent)
        self.assetName = assetName
        self.dragStartPos = None

    def mousePressEvent(self, event):
        if event.button() == QtCore.Qt.LeftButton:
            self.dragStartPos = event.pos()
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event):
        if not (event.buttons() & QtCore.Qt.LeftButton):
            return
        if not self.dragStartPos:
            return
        if (event.pos() - self.dragStartPos).manhattanLength() < QtWidgets.QApplication.startDragDistance():
            return

        drag = QtGui.QDrag(self)
        mimeData = QtCore.QMimeData()
        mimeData.setText(self.assetName)
        drag.setMimeData(mimeData)

        pixmap = self.pixmap()
        if pixmap and not pixmap.isNull():
            drag.setPixmap(pixmap.scaled(32, 32, QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation))
            drag.setHotSpot(QtCore.QPoint(16, 16))

        drag.exec_(QtCore.Qt.CopyAction)


class SegmentInspector(QtWidgets.QWidget):
    valueChanged = QtCore.pyqtSignal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.data = None
        self.assets = []

        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)

        self.groupBox = QtWidgets.QGroupBox(Locale.getContent("SEGMENT_PROPERTIES"))

        formLayout = QtWidgets.QFormLayout(self.groupBox)

        self.lblAsset = QtWidgets.QLabel()
        formLayout.addRow(Locale.getContent("asset"), self.lblAsset)

        self.startGroup = QtWidgets.QGroupBox(Locale.getContent("startFrame"))
        startLayout = QtWidgets.QFormLayout(self.startGroup)
        self.startTime = QtWidgets.QDoubleSpinBox()
        self.startTime.setRange(0, 9999)
        self.startTime.setSingleStep(0.1)
        self.startTime.valueChanged.connect(self._onValueChanged)
        startLayout.addRow(Locale.getContent("time"), self.startTime)

        posLayout = QtWidgets.QHBoxLayout()
        self.startX = QtWidgets.QDoubleSpinBox()
        self.startX.setRange(-9999, 9999)
        self.startX.setSingleStep(1.0)
        self.startX.valueChanged.connect(self._onValueChanged)
        self.startY = QtWidgets.QDoubleSpinBox()
        self.startY.setRange(-9999, 9999)
        self.startY.setSingleStep(1.0)
        self.startY.valueChanged.connect(self._onValueChanged)
        posLayout.addWidget(QtWidgets.QLabel("X"))
        posLayout.addWidget(self.startX)
        posLayout.addWidget(QtWidgets.QLabel("Y"))
        posLayout.addWidget(self.startY)
        startLayout.addRow(Locale.getContent("position"), posLayout)

        self.startRot = QtWidgets.QDoubleSpinBox()
        self.startRot.setRange(-360, 360)
        self.startRot.setSingleStep(5.0)
        self.startRot.valueChanged.connect(self._onValueChanged)
        startLayout.addRow(Locale.getContent("rotation"), self.startRot)

        scaleLayout = QtWidgets.QHBoxLayout()
        self.startSX = QtWidgets.QDoubleSpinBox()
        self.startSX.setRange(-99, 99)
        self.startSX.setSingleStep(0.1)
        self.startSX.valueChanged.connect(self._onValueChanged)
        self.startSY = QtWidgets.QDoubleSpinBox()
        self.startSY.setRange(-99, 99)
        self.startSY.setSingleStep(0.1)
        self.startSY.valueChanged.connect(self._onValueChanged)
        scaleLayout.addWidget(QtWidgets.QLabel("X"))
        scaleLayout.addWidget(self.startSX)
        scaleLayout.addWidget(QtWidgets.QLabel("Y"))
        scaleLayout.addWidget(self.startSY)
        startLayout.addRow(Locale.getContent("scale"), scaleLayout)

        formLayout.addRow(self.startGroup)

        self.endGroup = QtWidgets.QGroupBox(Locale.getContent("endFrame"))
        endLayout = QtWidgets.QFormLayout(self.endGroup)
        self.endTime = QtWidgets.QDoubleSpinBox()
        self.endTime.setRange(0, 9999)
        self.endTime.setSingleStep(0.1)
        self.endTime.valueChanged.connect(self._onValueChanged)
        endLayout.addRow(Locale.getContent("time"), self.endTime)

        posLayout2 = QtWidgets.QHBoxLayout()
        self.endX = QtWidgets.QDoubleSpinBox()
        self.endX.setRange(-9999, 9999)
        self.endX.setSingleStep(1.0)
        self.endX.valueChanged.connect(self._onValueChanged)
        self.endY = QtWidgets.QDoubleSpinBox()
        self.endY.setRange(-9999, 9999)
        self.endY.setSingleStep(1.0)
        self.endY.valueChanged.connect(self._onValueChanged)
        posLayout2.addWidget(QtWidgets.QLabel("X"))
        posLayout2.addWidget(self.endX)
        posLayout2.addWidget(QtWidgets.QLabel("Y"))
        posLayout2.addWidget(self.endY)
        endLayout.addRow(Locale.getContent("position"), posLayout2)

        self.endRot = QtWidgets.QDoubleSpinBox()
        self.endRot.setRange(-360, 360)
        self.endRot.setSingleStep(5.0)
        self.endRot.valueChanged.connect(self._onValueChanged)
        endLayout.addRow(Locale.getContent("rotation"), self.endRot)

        scaleLayout2 = QtWidgets.QHBoxLayout()
        self.endSX = QtWidgets.QDoubleSpinBox()
        self.endSX.setRange(-99, 99)
        self.endSX.setSingleStep(0.1)
        self.endSX.valueChanged.connect(self._onValueChanged)
        self.endSY = QtWidgets.QDoubleSpinBox()
        self.endSY.setRange(-99, 99)
        self.endSY.setSingleStep(0.1)
        self.endSY.valueChanged.connect(self._onValueChanged)
        scaleLayout2.addWidget(QtWidgets.QLabel("X"))
        scaleLayout2.addWidget(self.endSX)
        scaleLayout2.addWidget(QtWidgets.QLabel("Y"))
        scaleLayout2.addWidget(self.endSY)
        endLayout.addRow(Locale.getContent("scale"), scaleLayout2)

        formLayout.addRow(self.endGroup)

        layout.addWidget(self.groupBox)

        self.blockSignals = False
        self.groupBox.hide()

    def setSegment(self, segment: Dict, assets: List[str]):
        self.blockSignals = True
        self.data = segment
        self.assets = assets

        if not segment:
            self.groupBox.hide()
            self.blockSignals = False
            return

        self.groupBox.show()

        assetIdx = segment.get("asset", -1)
        if 0 <= assetIdx < len(assets):
            self.lblAsset.setText(assets[assetIdx])
        else:
            self.lblAsset.setText("Invalid")

        start = segment.get("startFrame", {})
        self.startTime.setValue(start.get("time", 0.0))
        pos = start.get("position", [0.0, 0.0])
        self.startX.setValue(pos[0])
        self.startY.setValue(pos[1])
        self.startRot.setValue(start.get("rotation", 0.0))
        scale = start.get("scale", [1.0, 1.0])
        self.startSX.setValue(scale[0])
        self.startSY.setValue(scale[1])

        end = segment.get("endFrame", {})
        self.endTime.setValue(end.get("time", 0.0))
        pos = end.get("position", [0.0, 0.0])
        self.endX.setValue(pos[0])
        self.endY.setValue(pos[1])
        self.endRot.setValue(end.get("rotation", 0.0))
        scale = end.get("scale", [1.0, 1.0])
        self.endSX.setValue(scale[0])
        self.endSY.setValue(scale[1])

        self.blockSignals = False

    def _onValueChanged(self):
        if self.blockSignals or not self.data:
            return

        start = self.data.setdefault("startFrame", {})
        start["time"] = self.startTime.value()
        start["position"] = [self.startX.value(), self.startY.value()]
        start["rotation"] = self.startRot.value()
        start["scale"] = [self.startSX.value(), self.startSY.value()]

        end = self.data.setdefault("endFrame", {})
        end["time"] = self.endTime.value()
        end["position"] = [self.endX.value(), self.endY.value()]
        end["rotation"] = self.endRot.value()
        end["scale"] = [self.endSX.value(), self.endSY.value()]

        self.valueChanged.emit()


class AnimationPreview(QtWidgets.QWidget):
    selectionChanged = QtCore.pyqtSignal(int, int)
    dataChanged = QtCore.pyqtSignal()
    segmentUpdated = QtCore.pyqtSignal(int, int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.data = {}
        self.currentTime = 0.0
        self.assetsCache = {}
        self.selectedSegment = None
        self.dragStartPos = None
        self.dragSegment = None
        self.dragMode = ""
        self.dragStartPositions = None
        self.dragAction = ""
        self.dragResizeEdgeX = ""
        self.dragResizeEdgeY = ""
        self.dragStartScales = None
        self.dragRotation = 0.0
        self.dragBaseWidth = 0.0
        self.dragBaseHeight = 0.0
        self.rotateActive = False
        self.rotateStartAngle = 0.0
        self.rotateStartRotations = None
        self.rotateCenter = None
        self.rotateMousePos = None
        self.setMouseTracking(True)

    def setData(self, data):
        self.data = data
        self.assetsCache.clear()
        self.selectedSegment = None
        self.dragStartPos = None
        self.dragSegment = None
        self.dragMode = ""
        self.dragStartPositions = None
        self.dragAction = ""
        self.dragResizeEdgeX = ""
        self.dragResizeEdgeY = ""
        self.dragStartScales = None
        self.dragRotation = 0.0
        self.dragBaseWidth = 0.0
        self.dragBaseHeight = 0.0
        self.rotateActive = False
        self.rotateStartAngle = 0.0
        self.rotateStartRotations = None
        self.rotateCenter = None
        self.rotateMousePos = None
        self.update()

    def setTime(self, time):
        self.currentTime = time
        self.update()

    def setSelectedSegment(self, trackIdx: int, segIdx: int):
        if trackIdx < 0 or segIdx < 0:
            self.selectedSegment = None
        else:
            self.selectedSegment = (trackIdx, segIdx)
        self.update()

    def paintEvent(self, event):
        painter = QtGui.QPainter(self)
        painter.fillRect(self.rect(), QtGui.QColor("#202020"))

        cx = self.width() / 2
        cy = self.height() / 2

        painter.setPen(QtGui.QPen(QtGui.QColor("#444"), 1, QtCore.Qt.DashLine))
        painter.drawLine(int(cx), 0, int(cx), self.height())
        painter.drawLine(0, int(cy), self.width(), int(cy))

        if not self.data:
            return

        timeLines = self.data.get("timeLines", [])
        assets = self.data.get("assets", [])

        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)
        painter.setRenderHint(QtGui.QPainter.SmoothPixmapTransform, True)

        for i, tl in enumerate(timeLines):
            segments = tl.get("timeSegments", [])
            for segIdx, seg in enumerate(segments):
                start = seg.get("startFrame", {})
                end = seg.get("endFrame", {})

                st = start.get("time", 0.0)
                et = end.get("time", 0.0)

                if st <= self.currentTime <= et:
                    if et - st <= 0.0001:
                        factor = 0.0
                    else:
                        factor = (self.currentTime - st) / (et - st)

                    sp = start.get("position", [0.0, 0.0])
                    ep = end.get("position", [0.0, 0.0])
                    curX = sp[0] + (ep[0] - sp[0]) * factor
                    curY = sp[1] + (ep[1] - sp[1]) * factor

                    sr = start.get("rotation", 0.0)
                    er = end.get("rotation", 0.0)
                    curRot = sr + (er - sr) * factor

                    ss = start.get("scale", [1.0, 1.0])
                    es = end.get("scale", [1.0, 1.0])
                    curSX = ss[0] + (es[0] - ss[0]) * factor
                    curSY = ss[1] + (es[1] - ss[1]) * factor

                    assetIdx = seg.get("asset", -1)
                    if 0 <= assetIdx < len(assets):
                        assetName = assets[assetIdx]
                        pixmap = self._getPixmap(assetName)
                        if pixmap:
                            painter.save()
                            painter.translate(cx + curX, cy + curY)
                            painter.rotate(curRot)
                            painter.scale(curSX, curSY)
                            painter.drawPixmap(-pixmap.width() // 2, -pixmap.height() // 2, pixmap)
                            painter.restore()

                            if self.selectedSegment == (i, segIdx):
                                transform = QtGui.QTransform()
                                transform.translate(cx + curX, cy + curY)
                                transform.rotate(curRot)
                                transform.scale(curSX, curSY)
                                rect = QtCore.QRectF(
                                    -pixmap.width() / 2, -pixmap.height() / 2, pixmap.width(), pixmap.height()
                                )
                                poly = transform.map(QtGui.QPolygonF(rect))
                                painter.setPen(QtGui.QPen(QtGui.QColor("#ffffff"), 2))
                                painter.setBrush(QtCore.Qt.NoBrush)
                                painter.drawPolygon(poly)

    def _getPixmap(self, name):
        if name in self.assetsCache:
            return self.assetsCache[name]

        path = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Animations", name)
        if os.path.exists(path):
            pm = QtGui.QPixmap(path)
            self.assetsCache[name] = pm
            return pm
        return None

    def getSegmentState(self, trackIdx, segIdx):
        timeLines = self.data.get("timeLines", [])
        if trackIdx < 0 or trackIdx >= len(timeLines):
            return None
        segments = timeLines[trackIdx].get("timeSegments", [])
        if segIdx < 0 or segIdx >= len(segments):
            return None
        seg = segments[segIdx]
        start = seg.get("startFrame", {})
        end = seg.get("endFrame", {})
        st = start.get("time", 0.0)
        et = end.get("time", 0.0)
        if not (st <= self.currentTime <= et):
            return None
        if et - st <= 0.0001:
            factor = 0.0
        else:
            factor = (self.currentTime - st) / (et - st)
        sp = start.get("position", [0.0, 0.0])
        ep = end.get("position", [0.0, 0.0])
        curX = sp[0] + (ep[0] - sp[0]) * factor
        curY = sp[1] + (ep[1] - sp[1]) * factor
        sr = start.get("rotation", 0.0)
        er = end.get("rotation", 0.0)
        curRot = sr + (er - sr) * factor
        ss = start.get("scale", [1.0, 1.0])
        es = end.get("scale", [1.0, 1.0])
        curSX = ss[0] + (es[0] - ss[0]) * factor
        curSY = ss[1] + (es[1] - ss[1]) * factor
        assetIdx = seg.get("asset", -1)
        assets = self.data.get("assets", [])
        if assetIdx < 0 or assetIdx >= len(assets):
            return None
        assetName = assets[assetIdx]
        pixmap = self._getPixmap(assetName)
        if not pixmap:
            return None
        return {
            "seg": seg,
            "start": start,
            "end": end,
            "curX": curX,
            "curY": curY,
            "curRot": curRot,
            "curSX": curSX,
            "curSY": curSY,
            "pixmap": pixmap,
        }

    def getSegmentPolygon(self, state, cx, cy):
        transform = QtGui.QTransform()
        transform.translate(cx + state["curX"], cy + state["curY"])
        transform.rotate(state["curRot"])
        transform.scale(state["curSX"], state["curSY"])
        pixmap = state["pixmap"]
        rect = QtCore.QRectF(-pixmap.width() / 2, -pixmap.height() / 2, pixmap.width(), pixmap.height())
        poly = transform.map(QtGui.QPolygonF(rect))
        return poly, transform

    def getResizeEdges(self, localPos, width, height, threshold):
        halfW = width / 2
        halfH = height / 2
        if localPos.x() < -halfW - threshold or localPos.x() > halfW + threshold:
            return "", ""
        if localPos.y() < -halfH - threshold or localPos.y() > halfH + threshold:
            return "", ""
        distLeft = abs(localPos.x() + halfW)
        distRight = abs(localPos.x() - halfW)
        distTop = abs(localPos.y() + halfH)
        distBottom = abs(localPos.y() - halfH)
        edgeX = ""
        edgeY = ""
        if distLeft <= threshold or distRight <= threshold:
            edgeX = "left" if distLeft <= distRight else "right"
        if distTop <= threshold or distBottom <= threshold:
            edgeY = "top" if distTop <= distBottom else "bottom"
        return edgeX, edgeY

    def mousePressEvent(self, event):
        if event.button() == QtCore.Qt.RightButton:
            if not self.data or not self.selectedSegment:
                return
            trackIdx, segIdx = self.selectedSegment
            state = self.getSegmentState(trackIdx, segIdx)
            if not state:
                return
            cx = self.width() / 2
            cy = self.height() / 2
            centerX = cx + state["curX"]
            centerY = cy + state["curY"]
            pos = QtCore.QPointF(event.pos())
            angle = math.degrees(math.atan2(pos.y() - centerY, pos.x() - centerX))
            seg = self.data.get("timeLines", [])[trackIdx].get("timeSegments", [])[segIdx]
            start = seg.get("startFrame", {})
            end = seg.get("endFrame", {})
            st = start.get("time", 0.0)
            et = end.get("time", 0.0)
            tolerance = 0.01
            if abs(self.currentTime - st) <= tolerance:
                self.dragMode = "start"
            elif abs(self.currentTime - et) <= tolerance:
                self.dragMode = "end"
            else:
                self.dragMode = "both"
            self.rotateActive = True
            self.rotateStartAngle = angle
            self.rotateStartRotations = (start.get("rotation", 0.0), end.get("rotation", 0.0))
            self.rotateCenter = (centerX, centerY)
            self.rotateMousePos = pos
            self.setCursor(QtCore.Qt.CrossCursor)
            self.update()
            return
        if event.button() != QtCore.Qt.LeftButton:
            return

        if not self.data:
            return

        timeLines = self.data.get("timeLines", [])
        assets = self.data.get("assets", [])
        if not timeLines or not assets:
            return

        cx = self.width() / 2
        cy = self.height() / 2
        clickPos = QtCore.QPointF(event.pos())

        selected = None

        for i in range(len(timeLines) - 1, -1, -1):
            segments = timeLines[i].get("timeSegments", [])
            for j in range(len(segments) - 1, -1, -1):
                state = self.getSegmentState(i, j)
                if not state:
                    continue
                poly, transform = self.getSegmentPolygon(state, cx, cy)
                if poly.containsPoint(clickPos, QtCore.Qt.WindingFill):
                    selected = (i, j)
                    break

            if selected:
                break

        if not selected and self.selectedSegment:
            trackIdx, segIdx = self.selectedSegment
            state = self.getSegmentState(trackIdx, segIdx)
            if state:
                poly, transform = self.getSegmentPolygon(state, cx, cy)
                inv, ok = transform.inverted()
                if ok:
                    localPos = inv.map(clickPos)
                    scaleMax = max(abs(state["curSX"]), abs(state["curSY"]), 0.01)
                    edgeX, edgeY = self.getResizeEdges(
                        localPos, state["pixmap"].width(), state["pixmap"].height(), 6 / scaleMax
                    )
                    if edgeX or edgeY:
                        selected = (trackIdx, segIdx)

        if selected:
            if self.selectedSegment != selected:
                self.selectedSegment = selected
                self.selectionChanged.emit(selected[0], selected[1])
                self.update()
            self.dragStartPos = QtCore.QPointF(event.pos())
            self.dragSegment = selected
            seg = timeLines[selected[0]].get("timeSegments", [])[selected[1]]
            start = seg.get("startFrame", {})
            end = seg.get("endFrame", {})
            st = start.get("time", 0.0)
            et = end.get("time", 0.0)
            tolerance = 0.01
            if abs(self.currentTime - st) <= tolerance:
                self.dragMode = "start"
            elif abs(self.currentTime - et) <= tolerance:
                self.dragMode = "end"
            else:
                self.dragMode = "both"
            sp = start.get("position", [0.0, 0.0])
            ep = end.get("position", [0.0, 0.0])
            self.dragStartPositions = (sp[0], sp[1], ep[0], ep[1])
            ss = start.get("scale", [1.0, 1.0])
            es = end.get("scale", [1.0, 1.0])
            self.dragStartScales = (ss[0], ss[1], es[0], es[1])
            state = self.getSegmentState(selected[0], selected[1])
            if state:
                poly, transform = self.getSegmentPolygon(state, cx, cy)
                inv, ok = transform.inverted()
                if ok:
                    localPos = inv.map(clickPos)
                    scaleMax = max(abs(state["curSX"]), abs(state["curSY"]), 0.01)
                    edgeX, edgeY = self.getResizeEdges(
                        localPos, state["pixmap"].width(), state["pixmap"].height(), 6 / scaleMax
                    )
                    if edgeX or edgeY:
                        self.dragAction = "scale"
                        self.dragResizeEdgeX = edgeX
                        self.dragResizeEdgeY = edgeY
                        self.dragRotation = state["curRot"]
                        self.dragBaseWidth = state["pixmap"].width()
                        self.dragBaseHeight = state["pixmap"].height()
                    else:
                        self.dragAction = "move"
                else:
                    self.dragAction = "move"
            else:
                self.dragAction = "move"
        else:
            if self.selectedSegment is not None:
                self.selectedSegment = None
                self.selectionChanged.emit(-1, -1)
                self.update()

    def mouseMoveEvent(self, event):
        if event.buttons() & QtCore.Qt.RightButton:
            if not self.rotateActive or not self.selectedSegment or not self.rotateStartRotations:
                return
            trackIdx, segIdx = self.selectedSegment
            timeLines = self.data.get("timeLines", [])
            if trackIdx >= len(timeLines):
                return
            segments = timeLines[trackIdx].get("timeSegments", [])
            if segIdx >= len(segments):
                return
            seg = segments[segIdx]
            start = seg.setdefault("startFrame", {})
            end = seg.setdefault("endFrame", {})
            if not self.rotateCenter:
                return
            centerX, centerY = self.rotateCenter
            pos = QtCore.QPointF(event.pos())
            angle = math.degrees(math.atan2(pos.y() - centerY, pos.x() - centerX))
            delta = angle - self.rotateStartAngle
            sr, er = self.rotateStartRotations
            if self.dragMode == "start":
                start["rotation"] = sr + delta
            elif self.dragMode == "end":
                end["rotation"] = er + delta
            else:
                start["rotation"] = sr + delta
                end["rotation"] = er + delta
            self.rotateMousePos = pos
            self.setCursor(QtCore.Qt.CrossCursor)
            self.segmentUpdated.emit(trackIdx, segIdx)
            self.update()
            return
        if not (event.buttons() & QtCore.Qt.LeftButton):
            if self.selectedSegment:
                trackIdx, segIdx = self.selectedSegment
                state = self.getSegmentState(trackIdx, segIdx)
                if state:
                    cx = self.width() / 2
                    cy = self.height() / 2
                    poly, transform = self.getSegmentPolygon(state, cx, cy)
                    inv, ok = transform.inverted()
                    if ok:
                        localPos = inv.map(QtCore.QPointF(event.pos()))
                        scaleMax = max(abs(state["curSX"]), abs(state["curSY"]), 0.01)
                        edgeX, edgeY = self.getResizeEdges(
                            localPos, state["pixmap"].width(), state["pixmap"].height(), 6 / scaleMax
                        )
                        if edgeX and edgeY:
                            if (edgeX == "left" and edgeY == "top") or (edgeX == "right" and edgeY == "bottom"):
                                self.setCursor(QtCore.Qt.SizeFDiagCursor)
                            else:
                                self.setCursor(QtCore.Qt.SizeBDiagCursor)
                        elif edgeX:
                            self.setCursor(QtCore.Qt.SizeHorCursor)
                        elif edgeY:
                            self.setCursor(QtCore.Qt.SizeVerCursor)
                        elif poly.containsPoint(QtCore.QPointF(event.pos()), QtCore.Qt.WindingFill):
                            self.setCursor(QtCore.Qt.SizeAllCursor)
                        else:
                            self.setCursor(QtCore.Qt.ArrowCursor)
                    else:
                        self.setCursor(QtCore.Qt.ArrowCursor)
                else:
                    self.setCursor(QtCore.Qt.ArrowCursor)
            else:
                self.setCursor(QtCore.Qt.ArrowCursor)
            return
        if not self.dragSegment or self.dragStartPos is None or not self.dragStartPositions:
            return

        dx = event.pos().x() - self.dragStartPos.x()
        dy = event.pos().y() - self.dragStartPos.y()
        trackIdx, segIdx = self.dragSegment
        timeLines = self.data.get("timeLines", [])
        if trackIdx >= len(timeLines):
            return
        segments = timeLines[trackIdx].get("timeSegments", [])
        if segIdx >= len(segments):
            return
        seg = segments[segIdx]
        start = seg.setdefault("startFrame", {})
        end = seg.setdefault("endFrame", {})
        spx, spy, epx, epy = self.dragStartPositions

        if self.dragAction == "scale" and self.dragStartScales:
            rad = math.radians(-self.dragRotation)
            localDx = dx * math.cos(rad) - dy * math.sin(rad)
            localDy = dx * math.sin(rad) + dy * math.cos(rad)
            ssx, ssy, esx, esy = self.dragStartScales
            deltaSx = 0.0
            deltaSy = 0.0
            if self.dragResizeEdgeX == "left":
                deltaSx = -localDx / max(1.0, self.dragBaseWidth)
            elif self.dragResizeEdgeX == "right":
                deltaSx = localDx / max(1.0, self.dragBaseWidth)
            if self.dragResizeEdgeY == "top":
                deltaSy = -localDy / max(1.0, self.dragBaseHeight)
            elif self.dragResizeEdgeY == "bottom":
                deltaSy = localDy / max(1.0, self.dragBaseHeight)

            if self.dragMode == "start":
                start["scale"] = [max(0.01, ssx + deltaSx), max(0.01, ssy + deltaSy)]
            elif self.dragMode == "end":
                end["scale"] = [max(0.01, esx + deltaSx), max(0.01, esy + deltaSy)]
            else:
                start["scale"] = [max(0.01, ssx + deltaSx), max(0.01, ssy + deltaSy)]
                end["scale"] = [max(0.01, esx + deltaSx), max(0.01, esy + deltaSy)]
        else:
            if self.dragMode == "start":
                start["position"] = [spx + dx, spy + dy]
            elif self.dragMode == "end":
                end["position"] = [epx + dx, epy + dy]
            else:
                start["position"] = [spx + dx, spy + dy]
                end["position"] = [epx + dx, epy + dy]

        self.segmentUpdated.emit(trackIdx, segIdx)
        self.update()

    def mouseReleaseEvent(self, event):
        if self.rotateActive and event.button() == QtCore.Qt.RightButton:
            self.rotateActive = False
            self.rotateStartAngle = 0.0
            self.rotateStartRotations = None
            self.rotateCenter = None
            self.rotateMousePos = None
            self.dragMode = ""
            self.unsetCursor()
            self.dataChanged.emit()
            self.update()
            return
        if self.dragSegment:
            self.dragStartPos = None
            self.dragSegment = None
            self.dragMode = ""
            self.dragStartPositions = None
            self.dragAction = ""
            self.dragResizeEdgeX = ""
            self.dragResizeEdgeY = ""
            self.dragStartScales = None
            self.dragRotation = 0.0
            self.dragBaseWidth = 0.0
            self.dragBaseHeight = 0.0
            self.dataChanged.emit()


class AnimationWindow(QtWidgets.QMainWindow):
    modified = QtCore.pyqtSignal()

    def __init__(
        self, parent: Optional[QtWidgets.QWidget] = None, title: str = "", data: Dict[str, Any] = None
    ) -> None:
        super().__init__(parent)
        self._data = data if data is not None else {}

        windowTitle = Locale.getContent("ANIMATION_WINDOW")
        self.title = title
        if title:
            windowTitle += f" - {title}"
        self.setWindowTitle(windowTitle)
        self.resize(1200, 900)

        central = QtWidgets.QWidget(self)
        self.setCentralWidget(central)
        mainLayout = QtWidgets.QHBoxLayout(central)

        leftPanel = QtWidgets.QWidget()
        leftLayout = QtWidgets.QVBoxLayout(leftPanel)
        leftLayout.setAlignment(QtCore.Qt.AlignTop)
        leftPanel.setFixedWidth(350)
        mainLayout.addWidget(leftPanel)

        rightPanel = QtWidgets.QWidget()
        mainLayout.addWidget(rightPanel, 1)
        rightLayout = QtWidgets.QVBoxLayout(rightPanel)
        rightLayout.setContentsMargins(0, 0, 0, 0)
        rightLayout.setSpacing(0)

        self.rightSplitter = QtWidgets.QSplitter(QtCore.Qt.Vertical)
        rightLayout.addWidget(self.rightSplitter)

        self.preview = AnimationPreview()
        self.preview.setData(self._data)
        self.preview.selectionChanged.connect(self.onPreviewSelectionChanged)
        self.preview.dataChanged.connect(self._onPreviewDataChanged)
        self.preview.segmentUpdated.connect(self.onPreviewSegmentUpdated)
        self.rightSplitter.addWidget(self.preview)

        self.timelinePanel = TimeLine()
        self.timelinePanel.setData(self._data)
        self.timelinePanel.dataChanged.connect(self._onTimelineChanged)
        self.timelinePanel.selectionChanged.connect(self._onSelectionChanged)
        self.timelinePanel.timeChanged.connect(self.preview.setTime)
        self.rightSplitter.addWidget(self.timelinePanel)

        self.rightSplitter.setStretchFactor(0, 1)
        self.rightSplitter.setStretchFactor(1, 1)
        self.rightSplitter.setSizes([600, 300])

        leftLayout.addWidget(QtWidgets.QLabel(Locale.getContent("ANIMATION_NAME")))
        self.nameEdit = QtWidgets.QLineEdit()
        self.nameEdit.setText(self._data.get("name", ""))
        self.nameEdit.textChanged.connect(self._onNameChanged)
        leftLayout.addWidget(self.nameEdit)

        leftLayout.addWidget(QtWidgets.QLabel(Locale.getContent("FRAME_RATE")))
        self.fpsCombo = QtWidgets.QComboBox()
        self.fpsCombo.addItems(["30", "60"])
        current_fps = str(self._data.get("frameRate", 30))
        index = self.fpsCombo.findText(current_fps)
        if index >= 0:
            self.fpsCombo.setCurrentIndex(index)
        self.fpsCombo.currentTextChanged.connect(self._onFpsChanged)
        leftLayout.addWidget(self.fpsCombo)

        leftLayout.addWidget(QtWidgets.QLabel(Locale.getContent("ASSETS")))

        self.assetsScroll = QtWidgets.QScrollArea()
        self.assetsScroll.setWidgetResizable(True)
        self.assetsScroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.assetsContainer = QtWidgets.QWidget()
        self.assetsContainer.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self.assetsContainer.customContextMenuRequested.connect(self._onAssetsContextMenu)
        self.assetsLayout = QtWidgets.QGridLayout(self.assetsContainer)
        self.assetsLayout.setAlignment(QtCore.Qt.AlignTop | QtCore.Qt.AlignLeft)
        self.assetsLayout.setSpacing(5)
        self.assetsScroll.setWidget(self.assetsContainer)
        leftLayout.addWidget(self.assetsScroll)

        self.inspector = SegmentInspector()
        self.inspector.valueChanged.connect(self._onInspectorValueChanged)
        leftLayout.addWidget(self.inspector)

        self._refreshAssets()

    def _onTimelineChanged(self):
        GameData.recordSnapshot()
        if self.title:
            GameData.animationsData[self.title] = copy.deepcopy(self._data)
        self.preview.update()
        self.modified.emit()

    def _onNameChanged(self, text: str) -> None:
        self._data["name"] = text
        GameData.recordSnapshot()
        GameData.animationsData[self.title] = copy.deepcopy(self._data)
        self.modified.emit()

    def _onFpsChanged(self, text: str) -> None:
        if text:
            self._data["frameRate"] = int(text)
            GameData.recordSnapshot()
            GameData.animationsData[self.title] = copy.deepcopy(self._data)
            self.timelinePanel.setData(self._data)
            self.modified.emit()

    def _onAssetsContextMenu(self, position: QtCore.QPoint) -> None:
        menu = QtWidgets.QMenu(self)

        actNew = QtWidgets.QAction(Locale.getContent("ADD_ASSET"), self)
        actNew.triggered.connect(self._onNewAsset)
        menu.addAction(actNew)

        actNewAudio = QtWidgets.QAction(Locale.getContent("ADD_AUDIO"), self)
        actNewAudio.triggered.connect(self._onNewAudioAsset)
        menu.addAction(actNewAudio)

        child = self.assetsContainer.childAt(position)
        if isinstance(child, QtWidgets.QLabel):
            assetName = child.property("assetName")
            if assetName:
                actDelete = QtWidgets.QAction(Locale.getContent("DELETE"), self)
                actDelete.triggered.connect(lambda: self._onDeleteAsset(assetName))
                menu.addAction(actDelete)

        menu.exec_(self.assetsContainer.mapToGlobal(position))

    def _addAssets(self, files: List[str]) -> None:
        GameData.recordSnapshot()
        assets = self._data.get("assets")
        if not isinstance(assets, list):
            assets = []
            self._data["assets"] = assets

        for f in files:
            name = os.path.basename(f)
            assets.append(name)

        if self.title:
            GameData.animationsData[self.title] = copy.deepcopy(self._data)
        self._refreshAssets()
        self.timelinePanel.setData(self._data)
        self.modified.emit()

    def _onNewAsset(self) -> None:
        startDir = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Animations")
        files, _ = QtWidgets.QFileDialog.getOpenFileNames(
            self, Locale.getContent("ADD_ASSET"), startDir, "Images (*.png *.jpg *.bmp)"
        )
        if files:
            self._addAssets(files)

    def _onNewAudioAsset(self) -> None:
        startDir = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Sounds")
        files, _ = QtWidgets.QFileDialog.getOpenFileNames(
            self, Locale.getContent("ADD_AUDIO"), startDir, "Audio (*.wav *.ogg *.mp3)"
        )
        if files:
            self._addAssets(files)

    def _onDeleteAsset(self, assetName: str) -> None:
        GameData.recordSnapshot()
        assets = self._data.get("assets")
        if isinstance(assets, list) and assetName in assets:
            index = assets.index(assetName)
            assets.pop(index)

            timeLines = self._data.get("timeLines", [])
            for tl in timeLines:
                segments = tl.get("timeSegments", [])
                for i in range(len(segments) - 1, -1, -1):
                    seg = segments[i]
                    segAssetIdx = seg.get("asset", -1)
                    if segAssetIdx == index:
                        segments.pop(i)
                    elif segAssetIdx > index:
                        seg["asset"] = segAssetIdx - 1

        if self.title:
            GameData.animationsData[self.title] = copy.deepcopy(self._data)
        self._refreshAssets()
        self.timelinePanel.setData(self._data)
        self.modified.emit()

    def _refreshAssets(self) -> None:
        while self.assetsLayout.count():
            item = self.assetsLayout.takeAt(0)
            widget = item.widget()
            if widget:
                widget.deleteLater()

        assets = self._data.get("assets", [])
        if not isinstance(assets, list):
            return

        col_count = 3
        for i, assetName in enumerate(assets):
            label = AssetLabel(assetName)
            label.setFixedSize(64, 64)
            label.setStyleSheet("border: 1px solid #555; background-color: #333;")
            label.setAlignment(QtCore.Qt.AlignCenter)
            label.setProperty("assetName", assetName)
            label.setToolTip(assetName)

            ext = os.path.splitext(assetName)[1].lower()
            if ext in [".png", ".jpg", ".bmp"]:
                assetPath = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Animations", assetName)
                if os.path.exists(assetPath):
                    pixmap = QtGui.QPixmap(assetPath)
                    if not pixmap.isNull():
                        if pixmap.width() > 64 or pixmap.height() > 64:
                            pixmap = pixmap.scaled(64, 64, QtCore.Qt.KeepAspectRatio, QtCore.Qt.SmoothTransformation)
                        label.setPixmap(pixmap)
                    else:
                        label.setText("Invalid")
                else:
                    label.setText("Missing")
            elif ext in [".wav", ".ogg", ".mp3"]:
                assetPath = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Sounds", assetName)
                if os.path.exists(assetPath):
                    label.setText("Audio")
                    label.setStyleSheet("border: 1px solid #555; background-color: #442222;")
                else:
                    label.setText("Missing")
            else:
                label.setText("Unknown")

            row = i // col_count
            col = i % col_count
            self.assetsLayout.addWidget(label, row, col)

    def _onSelectionChanged(self, trackIdx: int, segIdx: int) -> None:
        self.preview.setSelectedSegment(trackIdx, segIdx)
        if trackIdx == -1 or segIdx == -1:
            self.inspector.setSegment({}, [])
            return

        timeLines = self._data.get("timeLines", [])
        if 0 <= trackIdx < len(timeLines):
            segments = timeLines[trackIdx].get("timeSegments", [])
            if 0 <= segIdx < len(segments):
                seg = segments[segIdx]
                assets = self._data.get("assets", [])
                self.inspector.setSegment(seg, assets)

    def onPreviewSelectionChanged(self, trackIdx: int, segIdx: int) -> None:
        self.timelinePanel.setSelectedSegment(trackIdx, segIdx)

    def onPreviewSegmentUpdated(self, trackIdx: int, segIdx: int) -> None:
        self._onSelectionChanged(trackIdx, segIdx)

    def _onPreviewDataChanged(self) -> None:
        GameData.recordSnapshot()
        if self.title:
            GameData.animationsData[self.title] = copy.deepcopy(self._data)
        self.preview.update()
        self.modified.emit()

    def _onInspectorValueChanged(self) -> None:
        GameData.recordSnapshot()
        if self.title:
            GameData.animationsData[self.title] = copy.deepcopy(self._data)

        self.timelinePanel.canvas.updateCanvasSize()
        self.timelinePanel.canvas.update()
        self.preview.update()
        self.modified.emit()
