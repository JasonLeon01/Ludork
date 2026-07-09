# -*- encoding: utf-8 -*-

from __future__ import annotations

import copy
import math
from typing import Any, Dict, List, Optional, Tuple

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal import GameData

_INTERPOLATIONS = ("constant", "linear", "cubic")
_INFINITY_MODES = ("constant", "linear")
_KEY_HIT_RADIUS = 7.0
_TANGENT_HIT_RADIUS = 10.0
_TANGENT_HANDLE_LENGTH = 0.33
_MIN_VIEW_SPAN = 0.05


def _normaliseInterpolation(value: Any) -> str:
    mode = str(value)
    return mode if mode in _INTERPOLATIONS else "linear"


def _cubicHermite(
    startValue: float,
    endValue: float,
    leaveTangent: float,
    arriveTangent: float,
    alpha: float,
) -> float:
    t2 = alpha * alpha
    t3 = t2 * alpha
    return (
        (2.0 * t3 - 3.0 * t2 + 1.0) * startValue
        + (t3 - 2.0 * t2 + alpha) * leaveTangent
        + (-2.0 * t3 + 3.0 * t2) * endValue
        + (t3 - t2) * arriveTangent
    )


def _evaluateSegment(start: Dict[str, Any], end: Dict[str, Any], time: float) -> float:
    startTime = float(start["time"])
    endTime = float(end["time"])
    startValue = float(start["value"])
    endValue = float(end["value"])
    duration = endTime - startTime
    if duration <= 0.0:
        return endValue
    interpolation = str(start.get("interpolation", "linear"))
    if interpolation == "constant":
        return startValue
    alpha = (time - startTime) / duration
    if interpolation == "cubic":
        return _cubicHermite(
            startValue,
            endValue,
            float(start.get("leaveTangent", 0.0) or 0.0) * duration,
            float(end.get("arriveTangent", 0.0) or 0.0) * duration,
            alpha,
        )
    return startValue + (endValue - startValue) * alpha


def _extrapolate(
    time: float,
    start: Dict[str, Any],
    end: Dict[str, Any],
    mode: str,
    beforeFirst: bool,
) -> float:
    edgeKey = start if beforeFirst else end
    if mode != "linear":
        return float(edgeKey["value"])
    startTime = float(start["time"])
    endTime = float(end["time"])
    startValue = float(start["value"])
    endValue = float(end["value"])
    duration = endTime - startTime
    if duration <= 0.0:
        return float(edgeKey["value"])
    slope = (endValue - startValue) / duration
    return float(edgeKey["value"]) + (time - float(edgeKey["time"])) * slope


def _evaluateCurve(
    keys: List[Dict[str, Any]],
    defaultValue: float,
    preInfinity: str,
    postInfinity: str,
    time: float,
) -> float:
    if not keys:
        return defaultValue
    if len(keys) == 1:
        return float(keys[0]["value"])
    sampleTime = float(time)
    first = keys[0]
    last = keys[-1]
    if sampleTime <= float(first["time"]):
        return _extrapolate(sampleTime, first, keys[1], preInfinity, True)
    if sampleTime >= float(last["time"]):
        return _extrapolate(sampleTime, keys[-2], last, postInfinity, False)
    for index in range(len(keys) - 1):
        start = keys[index]
        end = keys[index + 1]
        startTime = float(start["time"])
        endTime = float(end["time"])
        if startTime <= sampleTime <= endTime:
            return _evaluateSegment(start, end, sampleTime)
    return float(last["value"])


def _normaliseKeys(value: Any) -> List[Dict[str, Any]]:
    if not isinstance(value, list):
        return []
    keys: List[Dict[str, Any]] = []
    for item in value:
        if not isinstance(item, dict):
            continue
        keys.append(
            {
                "time": float(item.get("time", 0.0) or 0.0),
                "value": float(item.get("value", 0.0) or 0.0),
                "interpolation": _normaliseInterpolation(item.get("interpolation", "linear")),
                "arriveTangent": float(item.get("arriveTangent", 0.0) or 0.0),
                "leaveTangent": float(item.get("leaveTangent", 0.0) or 0.0),
            }
        )
    return sorted(keys, key=lambda key: float(key["time"]))


def _sortKeysInPlace(keys: List[Dict[str, Any]]) -> None:
    keys.sort(key=lambda key: float(key["time"]))


def _tangentSpan(keys: List[Dict[str, Any]], index: int, side: str) -> float:
    if not 0 <= index < len(keys):
        return _TANGENT_HANDLE_LENGTH
    keyTime = float(keys[index]["time"])
    if side == "leave" and index < len(keys) - 1:
        return max((float(keys[index + 1]["time"]) - keyTime) / 3.0, _TANGENT_HANDLE_LENGTH)
    if side == "arrive" and index > 0:
        return max((keyTime - float(keys[index - 1]["time"])) / 3.0, _TANGENT_HANDLE_LENGTH)
    return _TANGENT_HANDLE_LENGTH


def _ensureAutoTangents(keys: List[Dict[str, Any]], index: int) -> None:
    if not 0 <= index < len(keys):
        return
    key = keys[index]
    if index > 0:
        prev = keys[index - 1]
        dt = float(key["time"]) - float(prev["time"])
        if dt > 1e-6:
            slope = (float(key["value"]) - float(prev["value"])) / dt
            if abs(float(key.get("arriveTangent", 0.0) or 0.0)) < 1e-9:
                key["arriveTangent"] = slope
            prev["interpolation"] = "cubic"
            if abs(float(prev.get("leaveTangent", 0.0) or 0.0)) < 1e-9:
                prev["leaveTangent"] = slope
    if index < len(keys) - 1:
        nxt = keys[index + 1]
        dt = float(nxt["time"]) - float(key["time"])
        if dt > 1e-6:
            slope = (float(nxt["value"]) - float(key["value"])) / dt
            key["interpolation"] = "cubic"
            if abs(float(key.get("leaveTangent", 0.0) or 0.0)) < 1e-9:
                key["leaveTangent"] = slope
            if abs(float(nxt.get("arriveTangent", 0.0) or 0.0)) < 1e-9:
                nxt["arriveTangent"] = slope


class CurveCanvas(QtWidgets.QWidget):
    DATA_CHANGED = QtCore.pyqtSignal()
    SELECTION_CHANGED = QtCore.pyqtSignal(int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._keys: List[Dict[str, Any]] = []
        self._defaultValue = 0.0
        self._preInfinity = "constant"
        self._postInfinity = "constant"
        self._selectedIndex = -1
        self._viewMinTime = -0.25
        self._viewMaxTime = 1.25
        self._viewMinValue = -0.25
        self._viewMaxValue = 1.25
        self._margin = 48
        self._dragMode = ""
        self._dragKeyIndex = -1
        self._dragTangentSide = ""
        self._dragStartPos = QtCore.QPoint()
        self.setMouseTracking(True)
        self.setFocusPolicy(QtCore.Qt.StrongFocus)
        self.setMinimumSize(320, 240)

    def setCurveData(
        self,
        keys: List[Dict[str, Any]],
        defaultValue: float,
        preInfinity: str,
        postInfinity: str,
    ) -> None:
        self._keys = _normaliseKeys(keys)
        self._defaultValue = float(defaultValue)
        self._preInfinity = preInfinity if preInfinity in _INFINITY_MODES else "constant"
        self._postInfinity = postInfinity if postInfinity in _INFINITY_MODES else "constant"
        if self._selectedIndex >= len(self._keys):
            self._selectedIndex = -1
        self.update()

    def getKeys(self) -> List[Dict[str, Any]]:
        return copy.deepcopy(self._keys)

    def selectedIndex(self) -> int:
        return self._selectedIndex

    def setSelectedIndex(self, index: int) -> None:
        if index == self._selectedIndex:
            return
        if index < -1 or index >= len(self._keys):
            index = -1
        self._selectedIndex = index
        self.SELECTION_CHANGED.emit(self._selectedIndex)
        self.update()

    def fitView(self) -> None:
        if not self._keys:
            self._viewMinTime = -0.25
            self._viewMaxTime = 1.25
            self._viewMinValue = -0.25
            self._viewMaxValue = 1.25
            self.update()
            return
        minTime = min(float(key["time"]) for key in self._keys)
        maxTime = max(float(key["time"]) for key in self._keys)
        minValue = min(float(key["value"]) for key in self._keys)
        maxValue = max(float(key["value"]) for key in self._keys)
        timeSpan = max(maxTime - minTime, _MIN_VIEW_SPAN)
        valueSpan = max(maxValue - minValue, _MIN_VIEW_SPAN)
        timePadding = timeSpan * 0.15
        valuePadding = valueSpan * 0.2
        self._viewMinTime = minTime - timePadding
        self._viewMaxTime = maxTime + timePadding
        self._viewMinValue = minValue - valuePadding
        self._viewMaxValue = maxValue + valuePadding
        self.update()

    def deleteSelectedKey(self) -> None:
        if self._selectedIndex < 0 or self._selectedIndex >= len(self._keys):
            return
        self._keys.pop(self._selectedIndex)
        self._selectedIndex = -1
        self.SELECTION_CHANGED.emit(-1)
        self.DATA_CHANGED.emit()
        self.update()

    def updateSelectedKey(self, updates: Dict[str, Any]) -> None:
        if self._selectedIndex < 0 or self._selectedIndex >= len(self._keys):
            return
        key = self._keys[self._selectedIndex]
        if "time" in updates:
            key["time"] = float(updates["time"])
        if "value" in updates:
            key["value"] = float(updates["value"])
        if "interpolation" in updates:
            newInterpolation = _normaliseInterpolation(updates["interpolation"])
            key["interpolation"] = newInterpolation
            if newInterpolation == "cubic":
                _ensureAutoTangents(self._keys, self._selectedIndex)
        if "arriveTangent" in updates:
            key["arriveTangent"] = float(updates["arriveTangent"])
        if "leaveTangent" in updates:
            key["leaveTangent"] = float(updates["leaveTangent"])
        _sortKeysInPlace(self._keys)
        self._selectedIndex = self._keys.index(key)
        self.SELECTION_CHANGED.emit(self._selectedIndex)
        self.DATA_CHANGED.emit()
        self.update()

    def _graphRect(self) -> QtCore.QRectF:
        return QtCore.QRectF(
            self._margin,
            self._margin,
            max(1.0, self.width() - self._margin * 2),
            max(1.0, self.height() - self._margin * 2),
        )

    def _timeToX(self, time: float) -> float:
        graph = self._graphRect()
        span = self._viewMaxTime - self._viewMinTime
        if span <= 0.0:
            return graph.left()
        return graph.left() + (time - self._viewMinTime) / span * graph.width()

    def _valueToY(self, value: float) -> float:
        graph = self._graphRect()
        span = self._viewMaxValue - self._viewMinValue
        if span <= 0.0:
            return graph.center().y()
        return graph.bottom() - (value - self._viewMinValue) / span * graph.height()

    def _xToTime(self, x: float) -> float:
        graph = self._graphRect()
        span = self._viewMaxTime - self._viewMinTime
        ratio = (x - graph.left()) / max(1.0, graph.width())
        return self._viewMinTime + ratio * span

    def _yToValue(self, y: float) -> float:
        graph = self._graphRect()
        span = self._viewMaxValue - self._viewMinValue
        ratio = (graph.bottom() - y) / max(1.0, graph.height())
        return self._viewMinValue + ratio * span

    def _clampKeyTime(self, index: int, time: float) -> float:
        minTime = self._viewMinTime
        maxTime = self._viewMaxTime
        if index > 0:
            minTime = max(minTime, float(self._keys[index - 1]["time"]) + 0.0001)
        if index < len(self._keys) - 1:
            maxTime = min(maxTime, float(self._keys[index + 1]["time"]) - 0.0001)
        return min(max(time, minTime), maxTime)

    def _tangentValue(self, index: int, side: str) -> float:
        key = self._keys[index]
        field = "leaveTangent" if side == "leave" else "arriveTangent"
        tangent = float(key.get(field, 0.0) or 0.0)
        if abs(tangent) > 1e-9:
            return tangent
        if side == "leave" and index < len(self._keys) - 1:
            nxt = self._keys[index + 1]
            dt = float(nxt["time"]) - float(key["time"])
            if dt > 1e-6:
                return (float(nxt["value"]) - float(key["value"])) / dt
        if side == "arrive" and index > 0:
            prev = self._keys[index - 1]
            dt = float(key["time"]) - float(prev["time"])
            if dt > 1e-6:
                return (float(key["value"]) - float(prev["value"])) / dt
        return 0.0

    def _tangentHandle(
        self, index: int, side: str
    ) -> Tuple[float, float]:
        key = self._keys[index]
        keyTime = float(key["time"])
        keyValue = float(key["value"])
        delta = _tangentSpan(self._keys, index, side)
        tangent = self._tangentValue(index, side)
        if side == "leave":
            return keyTime + delta, keyValue + tangent * delta
        return keyTime - delta, keyValue - tangent * delta

    def _hitTestKey(self, pos: QtCore.QPointF) -> int:
        for index in range(len(self._keys) - 1, -1, -1):
            key = self._keys[index]
            dx = pos.x() - self._timeToX(float(key["time"]))
            dy = pos.y() - self._valueToY(float(key["value"]))
            if math.hypot(dx, dy) <= _KEY_HIT_RADIUS:
                return index
        return -1

    def _hitTestTangent(self, pos: QtCore.QPointF, index: int) -> str:
        if index < 0 or index >= len(self._keys):
            return ""
        for side in ("leave", "arrive"):
            if side == "arrive" and index <= 0:
                continue
            if side == "leave" and index >= len(self._keys) - 1:
                continue
            handleTime, handleValue = self._tangentHandle(index, side)
            dx = pos.x() - self._timeToX(handleTime)
            dy = pos.y() - self._valueToY(handleValue)
            if math.hypot(dx, dy) <= _TANGENT_HIT_RADIUS:
                return side
        return ""

    def _addKeyAt(self, time: float, value: float) -> None:
        newKey = {
            "time": time,
            "value": value,
            "interpolation": "linear",
            "arriveTangent": 0.0,
            "leaveTangent": 0.0,
        }
        self._keys.append(newKey)
        _sortKeysInPlace(self._keys)
        self._selectedIndex = self._keys.index(newKey)
        self.SELECTION_CHANGED.emit(self._selectedIndex)
        self.DATA_CHANGED.emit()
        self.update()

    def wheelEvent(self, event: QtGui.QWheelEvent) -> None:
        graph = self._graphRect()
        if not graph.contains(event.pos()):
            super().wheelEvent(event)
            return
        factor = 0.9 if event.angleDelta().y() > 0 else 1.1
        modifiers = event.modifiers()
        if modifiers & QtCore.Qt.ControlModifier:
            anchorValue = self._yToValue(event.pos().y())
            span = self._viewMaxValue - self._viewMinValue
            newSpan = max(_MIN_VIEW_SPAN, span * factor)
            ratio = (anchorValue - self._viewMinValue) / max(span, 1e-6)
            self._viewMinValue = anchorValue - newSpan * ratio
            self._viewMaxValue = self._viewMinValue + newSpan
        else:
            anchorTime = self._xToTime(event.pos().x())
            span = self._viewMaxTime - self._viewMinTime
            newSpan = max(_MIN_VIEW_SPAN, span * factor)
            ratio = (anchorTime - self._viewMinTime) / max(span, 1e-6)
            self._viewMinTime = anchorTime - newSpan * ratio
            self._viewMaxTime = self._viewMinTime + newSpan
        self.update()
        event.accept()

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        pos = event.pos()
        if event.button() == QtCore.Qt.MiddleButton:
            self._dragMode = "pan"
            self._dragStartPos = pos
            self.setCursor(QtCore.Qt.ClosedHandCursor)
            event.accept()
            return
        if event.button() != QtCore.Qt.LeftButton:
            super().mousePressEvent(event)
            return
        if self._selectedIndex >= 0:
            tangentSide = self._hitTestTangent(pos, self._selectedIndex)
            if tangentSide:
                _ensureAutoTangents(self._keys, self._selectedIndex)
                self._dragMode = "tangent"
                self._dragTangentSide = tangentSide
                self._dragKeyIndex = self._selectedIndex
                self._dragStartPos = pos
                self.update()
                event.accept()
                return
        keyIndex = self._hitTestKey(pos)
        if keyIndex >= 0:
            self._selectedIndex = keyIndex
            self.SELECTION_CHANGED.emit(self._selectedIndex)
            tangentSide = self._hitTestTangent(pos, keyIndex)
            if tangentSide:
                _ensureAutoTangents(self._keys, keyIndex)
                self._dragMode = "tangent"
                self._dragTangentSide = tangentSide
                self._dragKeyIndex = keyIndex
            else:
                self._dragMode = "key"
                self._dragKeyIndex = keyIndex
            self._dragStartPos = pos
            self.update()
            event.accept()
            return
        self._selectedIndex = -1
        self.SELECTION_CHANGED.emit(-1)
        self.update()
        super().mousePressEvent(event)

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        pos = event.pos()
        if self._dragMode == "pan":
            delta = pos - self._dragStartPos
            timeSpan = self._viewMaxTime - self._viewMinTime
            valueSpan = self._viewMaxValue - self._viewMinValue
            graph = self._graphRect()
            self._viewMinTime -= delta.x() / max(1.0, graph.width()) * timeSpan
            self._viewMaxTime -= delta.x() / max(1.0, graph.width()) * timeSpan
            self._viewMinValue += delta.y() / max(1.0, graph.height()) * valueSpan
            self._viewMaxValue += delta.y() / max(1.0, graph.height()) * valueSpan
            self._dragStartPos = pos
            self.update()
            event.accept()
            return
        if self._dragMode == "key" and 0 <= self._dragKeyIndex < len(self._keys):
            index = self._dragKeyIndex
            newTime = self._clampKeyTime(index, self._xToTime(pos.x()))
            newValue = self._yToValue(pos.y())
            key = self._keys[index]
            key["time"] = newTime
            key["value"] = newValue
            self._selectedIndex = index
            self.DATA_CHANGED.emit()
            self.update()
            event.accept()
            return
        if self._dragMode == "tangent" and 0 <= self._dragKeyIndex < len(self._keys):
            index = self._dragKeyIndex
            key = self._keys[index]
            keyTime = float(key["time"])
            keyValue = float(key["value"])
            handleTime = self._xToTime(pos.x())
            handleValue = self._yToValue(pos.y())
            if self._dragTangentSide == "leave":
                deltaTime = handleTime - keyTime
                if abs(deltaTime) > 1e-6:
                    key["leaveTangent"] = (handleValue - keyValue) / deltaTime
                    key["interpolation"] = "cubic"
            else:
                deltaTime = keyTime - handleTime
                if abs(deltaTime) > 1e-6:
                    key["arriveTangent"] = (keyValue - handleValue) / deltaTime
                    if index > 0:
                        self._keys[index - 1]["interpolation"] = "cubic"
            self.DATA_CHANGED.emit()
            self.update()
            event.accept()
            return
        hoveredTangent = False
        if self._selectedIndex >= 0:
            hoveredTangent = self._hitTestTangent(pos, self._selectedIndex) != ""
        hoveredKey = self._hitTestKey(pos) >= 0
        if hoveredTangent:
            self.setCursor(QtCore.Qt.SizeAllCursor)
        elif hoveredKey:
            self.setCursor(QtCore.Qt.PointingHandCursor)
        else:
            self.setCursor(QtCore.Qt.ArrowCursor)

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent) -> None:
        if self._dragMode:
            self._dragMode = ""
            self._dragKeyIndex = -1
            self._dragTangentSide = ""
            self.setCursor(QtCore.Qt.ArrowCursor)
            event.accept()
            return
        super().mouseReleaseEvent(event)

    def mouseDoubleClickEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() != QtCore.Qt.LeftButton:
            super().mouseDoubleClickEvent(event)
            return
        graph = self._graphRect()
        if not graph.contains(event.pos()):
            super().mouseDoubleClickEvent(event)
            return
        if self._hitTestKey(event.pos()) >= 0:
            super().mouseDoubleClickEvent(event)
            return
        time = self._xToTime(event.pos().x())
        value = _evaluateCurve(
            self._keys,
            self._defaultValue,
            self._preInfinity,
            self._postInfinity,
            time,
        )
        self._addKeyAt(time, value)
        event.accept()

    def keyPressEvent(self, event: QtGui.QKeyEvent) -> None:
        if event.key() in (QtCore.Qt.Key_Delete, QtCore.Qt.Key_Backspace):
            self.deleteSelectedKey()
            event.accept()
            return
        super().keyPressEvent(event)

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.Antialiasing)
        painter.fillRect(self.rect(), QtGui.QColor("#2b2b2b"))
        graph = self._graphRect()
        painter.fillRect(graph.toRect(), QtGui.QColor("#242424"))
        self._drawGrid(painter, graph)
        self._drawCurve(painter, graph)
        self._drawKeys(painter)
        self._drawHint(painter)

    def _drawGrid(self, painter: QtGui.QPainter, graph: QtCore.QRectF) -> None:
        painter.setFont(QtGui.QFont("Segoe UI", 8))
        timeStep = self._niceStep(self._viewMaxTime - self._viewMinTime)
        valueStep = self._niceStep(self._viewMaxValue - self._viewMinValue)
        time = math.floor(self._viewMinTime / timeStep) * timeStep
        while time <= self._viewMaxTime + timeStep * 0.5:
            x = self._timeToX(time)
            if graph.left() <= x <= graph.right():
                painter.setPen(QtGui.QPen(QtGui.QColor("#3a3a3a"), 1))
                painter.drawLine(QtCore.QPointF(x, graph.top()), QtCore.QPointF(x, graph.bottom()))
                painter.setPen(QtGui.QColor("#9a9a9a"))
                painter.drawText(QtCore.QRectF(x + 2, graph.bottom() + 4, 40, 16), f"{time:.2g}")
            time += timeStep
        value = math.floor(self._viewMinValue / valueStep) * valueStep
        while value <= self._viewMaxValue + valueStep * 0.5:
            y = self._valueToY(value)
            if graph.top() <= y <= graph.bottom():
                painter.setPen(QtGui.QPen(QtGui.QColor("#3a3a3a"), 1))
                painter.drawLine(QtCore.QPointF(graph.left(), y), QtCore.QPointF(graph.right(), y))
                painter.setPen(QtGui.QColor("#9a9a9a"))
                painter.drawText(QtCore.QRectF(4, y - 8, self._margin - 8, 16), QtCore.Qt.AlignRight, f"{value:.2g}")
            value += valueStep
        painter.setPen(QtGui.QPen(QtGui.QColor("#666666"), 1))
        painter.drawRect(graph)

    def _niceStep(self, span: float) -> float:
        if span <= 0.0:
            return 1.0
        exponent = math.floor(math.log10(span))
        fraction = span / (10 ** exponent)
        if fraction < 1.5:
            nice = 1.0
        elif fraction < 3.0:
            nice = 2.0
        elif fraction < 7.0:
            nice = 5.0
        else:
            nice = 10.0
        return nice * (10 ** exponent)

    def _drawCurve(self, painter: QtGui.QPainter, graph: QtCore.QRectF) -> None:
        if not self._keys:
            return
        sampleCount = max(32, int(graph.width()))
        startTime = self._viewMinTime
        endTime = self._viewMaxTime
        path = QtGui.QPainterPath()
        firstValue = _evaluateCurve(
            self._keys,
            self._defaultValue,
            self._preInfinity,
            self._postInfinity,
            startTime,
        )
        path.moveTo(self._timeToX(startTime), self._valueToY(firstValue))
        for index in range(1, sampleCount + 1):
            alpha = index / sampleCount
            time = startTime + (endTime - startTime) * alpha
            value = _evaluateCurve(
                self._keys,
                self._defaultValue,
                self._preInfinity,
                self._postInfinity,
                time,
            )
            path.lineTo(self._timeToX(time), self._valueToY(value))
        painter.setPen(QtGui.QPen(QtGui.QColor("#f0a020"), 2))
        painter.drawPath(path)

    def _drawKeys(self, painter: QtGui.QPainter) -> None:
        if 0 <= self._selectedIndex < len(self._keys):
            index = self._selectedIndex
            if index > 0:
                self._drawTangent(painter, index, "arrive")
            if index < len(self._keys) - 1:
                self._drawTangent(painter, index, "leave")
        for index, key in enumerate(self._keys):
            x = self._timeToX(float(key["time"]))
            y = self._valueToY(float(key["value"]))
            selected = index == self._selectedIndex
            size = 5.0 if selected else 4.0
            diamond = QtGui.QPolygonF(
                [
                    QtCore.QPointF(x, y - size),
                    QtCore.QPointF(x + size, y),
                    QtCore.QPointF(x, y + size),
                    QtCore.QPointF(x - size, y),
                ]
            )
            painter.setPen(QtGui.QPen(QtGui.QColor("#ffffff" if selected else "#dddddd"), 1))
            painter.setBrush(QtGui.QColor("#f0a020" if selected else "#c88412"))
            painter.drawPolygon(diamond)

    def _drawTangent(self, painter: QtGui.QPainter, index: int, side: str) -> None:
        key = self._keys[index]
        keyX = self._timeToX(float(key["time"]))
        keyY = self._valueToY(float(key["value"]))
        handleTime, handleValue = self._tangentHandle(index, side)
        handleX = self._timeToX(handleTime)
        handleY = self._valueToY(handleValue)
        painter.setPen(QtGui.QPen(QtGui.QColor("#6ab4ff"), 1, QtCore.Qt.DashLine))
        painter.drawLine(QtCore.QPointF(keyX, keyY), QtCore.QPointF(handleX, handleY))
        painter.setPen(QtCore.Qt.NoPen)
        painter.setBrush(QtGui.QColor("#6ab4ff"))
        painter.drawEllipse(QtCore.QPointF(handleX, handleY), 4.0, 4.0)

    def _drawHint(self, painter: QtGui.QPainter) -> None:
        painter.setPen(QtGui.QColor("#777777"))
        painter.setFont(QtGui.QFont("Segoe UI", 8))
        painter.drawText(
            QtCore.QRectF(self._margin, 6, self.width() - self._margin * 2, 16),
            QtCore.Qt.AlignRight,
            ELOC("CURVE_CANVAS_HINT"),
        )


class CurveKeyInspector(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal()

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._syncing = False
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        self.groupBox = QtWidgets.QGroupBox(ELOC("CURVE_KEY_PROPERTIES"))
        form = QtWidgets.QFormLayout(self.groupBox)
        self.timeSpin = QtWidgets.QDoubleSpinBox()
        self.timeSpin.setRange(-1000000000.0, 1000000000.0)
        self.timeSpin.setDecimals(6)
        self.timeSpin.valueChanged.connect(self._onValueChanged)
        form.addRow(ELOC("time"), self.timeSpin)
        self.valueSpin = QtWidgets.QDoubleSpinBox()
        self.valueSpin.setRange(-1000000000.0, 1000000000.0)
        self.valueSpin.setDecimals(6)
        self.valueSpin.valueChanged.connect(self._onValueChanged)
        form.addRow(ELOC("CURVE_VALUE"), self.valueSpin)
        self.interpolation = QtWidgets.QComboBox()
        self.interpolation.addItems(list(_INTERPOLATIONS))
        self.interpolation.currentTextChanged.connect(self._onValueChanged)
        form.addRow(ELOC("CURVE_INTERPOLATION"), self.interpolation)
        self.arriveTangent = QtWidgets.QDoubleSpinBox()
        self.arriveTangent.setRange(-1000000000.0, 1000000000.0)
        self.arriveTangent.setDecimals(6)
        self.arriveTangent.valueChanged.connect(self._onValueChanged)
        form.addRow(ELOC("CURVE_ARRIVE_TANGENT"), self.arriveTangent)
        self.leaveTangent = QtWidgets.QDoubleSpinBox()
        self.leaveTangent.setRange(-1000000000.0, 1000000000.0)
        self.leaveTangent.setDecimals(6)
        self.leaveTangent.valueChanged.connect(self._onValueChanged)
        form.addRow(ELOC("CURVE_LEAVE_TANGENT"), self.leaveTangent)
        layout.addWidget(self.groupBox)
        self.emptyLabel = QtWidgets.QLabel(ELOC("NO_SELECTION"))
        self.emptyLabel.setAlignment(QtCore.Qt.AlignCenter)
        layout.addWidget(self.emptyLabel)
        layout.addStretch(1)
        self.setKey(None)

    def setKey(self, key: Optional[Dict[str, Any]]) -> None:
        self._syncing = True
        hasKey = key is not None
        self.groupBox.setVisible(hasKey)
        self.emptyLabel.setVisible(not hasKey)
        if hasKey and key is not None:
            self.timeSpin.setValue(float(key.get("time", 0.0) or 0.0))
            self.valueSpin.setValue(float(key.get("value", 0.0) or 0.0))
            interpolation = _normaliseInterpolation(key.get("interpolation", "linear"))
            index = self.interpolation.findText(interpolation)
            if index >= 0:
                self.interpolation.setCurrentIndex(index)
            self.arriveTangent.setValue(float(key.get("arriveTangent", 0.0) or 0.0))
            self.leaveTangent.setValue(float(key.get("leaveTangent", 0.0) or 0.0))
            cubic = interpolation == "cubic"
            self.arriveTangent.setEnabled(cubic)
            self.leaveTangent.setEnabled(cubic)
        self._syncing = False

    def _onValueChanged(self, *_args: Any) -> None:
        if self._syncing:
            return
        cubic = self.interpolation.currentText() == "cubic"
        self.arriveTangent.setEnabled(cubic)
        self.leaveTangent.setEnabled(cubic)
        self.VALUE_CHANGED.emit()


class CurveEditor(QtWidgets.QWidget):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(
        self, parent: Optional[QtWidgets.QWidget] = None, title: str = "", data: Optional[Dict[str, Any]] = None
    ) -> None:
        super().__init__(parent)
        self.title = title
        self._data = copy.deepcopy(data) if isinstance(data, dict) else {}
        self._syncing = False

        layout = QtWidgets.QVBoxLayout(self)

        form = QtWidgets.QFormLayout()
        self.nameEdit = QtWidgets.QLineEdit()
        self.nameEdit.setText(str(self._data.get("name", "")))
        self.nameEdit.textChanged.connect(self._onNameChanged)
        form.addRow(ELOC("CURVE_NAME"), self.nameEdit)

        self.defaultValue = QtWidgets.QDoubleSpinBox()
        self.defaultValue.setRange(-1000000000.0, 1000000000.0)
        self.defaultValue.setDecimals(6)
        self.defaultValue.setValue(float(self._data.get("defaultValue", 0.0) or 0.0))
        self.defaultValue.valueChanged.connect(self._onDefaultValueChanged)
        form.addRow(ELOC("CURVE_DEFAULT_VALUE"), self.defaultValue)

        self.preInfinity = self._makeCombo(_INFINITY_MODES, str(self._data.get("preInfinity", "constant")))
        self.preInfinity.currentTextChanged.connect(lambda text: self._onInfinityChanged("preInfinity", text))
        form.addRow(ELOC("CURVE_PRE_INFINITY"), self.preInfinity)

        self.postInfinity = self._makeCombo(_INFINITY_MODES, str(self._data.get("postInfinity", "constant")))
        self.postInfinity.currentTextChanged.connect(lambda text: self._onInfinityChanged("postInfinity", text))
        form.addRow(ELOC("CURVE_POST_INFINITY"), self.postInfinity)
        layout.addLayout(form)

        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal)
        self.canvas = CurveCanvas()
        self.canvas.DATA_CHANGED.connect(self._onCanvasChanged)
        self.canvas.SELECTION_CHANGED.connect(self._onCanvasSelectionChanged)
        splitter.addWidget(self.canvas)
        self.keyInspector = CurveKeyInspector()
        self.keyInspector.VALUE_CHANGED.connect(self._onInspectorChanged)
        splitter.addWidget(self.keyInspector)
        splitter.setStretchFactor(0, 1)
        splitter.setStretchFactor(1, 0)
        splitter.setSizes([560, 220])
        layout.addWidget(splitter, 1)

        buttons = QtWidgets.QHBoxLayout()
        self.fitButton = QtWidgets.QPushButton(ELOC("CURVE_FIT_VIEW"))
        self.fitButton.clicked.connect(self._onFitView)
        buttons.addWidget(self.fitButton)
        self.deleteButton = QtWidgets.QPushButton(ELOC("DELETE"))
        self.deleteButton.clicked.connect(self._onDeleteKey)
        buttons.addWidget(self.deleteButton)
        buttons.addStretch(1)
        layout.addLayout(buttons)

        self._normaliseData()
        self._refreshCanvas()
        self.canvas.fitView()

    def _makeCombo(self, values: tuple[str, ...], current: str) -> QtWidgets.QComboBox:
        combo = QtWidgets.QComboBox()
        combo.addItems(list(values))
        index = combo.findText(current)
        if index >= 0:
            combo.setCurrentIndex(index)
        return combo

    def _normaliseData(self) -> None:
        self._data.setdefault("name", self.title.rsplit("/", 1)[-1] if self.title else "")
        self._data.setdefault("defaultValue", 0.0)
        if self._data.get("preInfinity") not in _INFINITY_MODES:
            self._data["preInfinity"] = "constant"
        if self._data.get("postInfinity") not in _INFINITY_MODES:
            self._data["postInfinity"] = "constant"
        self._data["keys"] = _normaliseKeys(self._data.get("keys", []))

    def _refreshCanvas(self) -> None:
        keys = self._data.get("keys", [])
        if not isinstance(keys, list):
            keys = []
        self.canvas.setCurveData(
            keys,
            float(self._data.get("defaultValue", 0.0) or 0.0),
            str(self._data.get("preInfinity", "constant")),
            str(self._data.get("postInfinity", "constant")),
        )
        self._refreshInspector()

    def _refreshInspector(self) -> None:
        index = self.canvas.selectedIndex()
        keys = self._data.get("keys", [])
        if not isinstance(keys, list) or index < 0 or index >= len(keys):
            self.keyInspector.setKey(None)
            return
        key = keys[index]
        if isinstance(key, dict):
            self.keyInspector.setKey(key)
        else:
            self.keyInspector.setKey(None)

    def _syncKeysFromCanvas(self) -> None:
        self._data["keys"] = self.canvas.getKeys()

    def _commit(self) -> None:
        if self._syncing or not self.title:
            return
        self._normaliseData()
        GameData.RecordSnapshot()
        GameData.curvesData[self.title] = copy.deepcopy(self._data)
        self.MODIFIED.emit()

    def _onNameChanged(self, text: str) -> None:
        self._data["name"] = text
        self._commit()

    def _onDefaultValueChanged(self, value: float) -> None:
        self._data["defaultValue"] = float(value)
        self._refreshCanvas()
        self._commit()

    def _onInfinityChanged(self, key: str, value: str) -> None:
        if value in _INFINITY_MODES:
            self._data[key] = value
            self._refreshCanvas()
            self._commit()

    def _onCanvasChanged(self) -> None:
        if self._syncing:
            return
        self._syncKeysFromCanvas()
        self._refreshInspector()
        self._commit()

    def _onCanvasSelectionChanged(self, index: int) -> None:
        if self._syncing:
            return
        self._refreshInspector()

    def _onInspectorChanged(self) -> None:
        if self._syncing:
            return
        index = self.canvas.selectedIndex()
        if index < 0:
            return
        self.canvas.updateSelectedKey(
            {
                "time": self.keyInspector.timeSpin.value(),
                "value": self.keyInspector.valueSpin.value(),
                "interpolation": self.keyInspector.interpolation.currentText(),
                "arriveTangent": self.keyInspector.arriveTangent.value(),
                "leaveTangent": self.keyInspector.leaveTangent.value(),
            }
        )

    def _onFitView(self) -> None:
        self.canvas.fitView()

    def _onDeleteKey(self) -> None:
        self.canvas.deleteSelectedKey()
        self._syncKeysFromCanvas()
        self._refreshInspector()
        self._commit()

    def reloadData(self, title: str, data: Dict[str, Any]) -> None:
        self.title = title
        self._data = copy.deepcopy(data)
        self._normaliseData()
        self._syncing = True
        self.nameEdit.setText(str(self._data.get("name", "")))
        self.defaultValue.setValue(float(self._data.get("defaultValue", 0.0) or 0.0))
        for combo, key in ((self.preInfinity, "preInfinity"), (self.postInfinity, "postInfinity")):
            index = combo.findText(str(self._data.get(key, "constant")))
            if index >= 0:
                combo.setCurrentIndex(index)
        self._syncing = False
        self._refreshCanvas()
        self.canvas.fitView()


class CurveWindow(QtWidgets.QMainWindow):
    MODIFIED = QtCore.pyqtSignal()

    def __init__(
        self, parent: Optional[QtWidgets.QWidget] = None, title: str = "", data: Optional[Dict[str, Any]] = None
    ) -> None:
        super().__init__(parent)
        self.setWindowFlags(QtCore.Qt.Window)
        windowTitle = ELOC("CURVE_WINDOW")
        if title:
            windowTitle += f" - {title}"
        self.setWindowTitle(windowTitle)
        self.resize(900, 620)
        self._editor = CurveEditor(self, title, data)
        self._editor.MODIFIED.connect(self.MODIFIED.emit)
        self.setCentralWidget(self._editor)

    def reloadData(self, title: str, data: Dict[str, Any]) -> None:
        windowTitle = ELOC("CURVE_WINDOW")
        if title:
            windowTitle += f" - {title}"
        self.setWindowTitle(windowTitle)
        self._editor.reloadData(title, data)
