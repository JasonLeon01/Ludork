# -*- encoding: utf-8 -*-

from __future__ import annotations

import ast
import os
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple, cast

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal import EditorStatus, GameData
from Utils import File, System
from .DialogUtils import GetIndependentDialogParent, IsWidgetValid
from ._MapViewBase import MapRenderViewBase


@dataclass
class _RouteMapData:
    key: str
    name: str
    width: int
    height: int
    layers: Dict[str, Any]


class MoveRouteMapView(MapRenderViewBase):
    ROUTE_CHANGED = QtCore.pyqtSignal(object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(MoveRouteMapView, self).__init__(parent)
        self._mapData: Optional[_RouteMapData] = None
        self._dragging = False
        self._startCell: Optional[Tuple[int, int]] = None
        self._currentCell: Optional[Tuple[int, int]] = None
        self._routeCells: List[Tuple[int, int]] = []
        self._routeSteps: List[Tuple[int, int]] = []

    def setMapData(self, mapData: Optional[_RouteMapData]) -> None:
        self._mapData = mapData
        self.clearRoute()
        self._renderMap()
        self._updateContentSize()
        self.update()

    def getRoute(self) -> List[Tuple[int, int]]:
        return [(step[0], step[1]) for step in self._routeSteps]

    def setRoute(self, route: Any) -> None:
        self._routeSteps = _normaliseRoute(route)
        self._startCell = None
        self._currentCell = None
        self._routeCells = []
        self.update()

    def clearRoute(self) -> None:
        self._dragging = False
        self._startCell = None
        self._currentCell = None
        self._routeCells = []
        self._routeSteps = []
        self.ROUTE_CHANGED.emit(self.getRoute())

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() != QtCore.Qt.LeftButton:
            super(MoveRouteMapView, self).mousePressEvent(event)
            return
        cell = self._cellFromPos(event.pos())
        if cell is None:
            return
        self._dragging = True
        self._startCell = cell
        self._currentCell = cell
        self._routeCells = [cell]
        self._routeSteps = []
        self.ROUTE_CHANGED.emit(self.getRoute())
        self.update()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        if not self._dragging or not (event.buttons() & QtCore.Qt.LeftButton):
            super(MoveRouteMapView, self).mouseMoveEvent(event)
            return
        cell = self._cellFromPos(event.pos())
        if cell is None:
            return
        self._appendPathTo(cell)

    def mouseReleaseEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() == QtCore.Qt.LeftButton and self._dragging:
            cell = self._cellFromPos(event.pos())
            if cell is not None:
                self._appendPathTo(cell)
            self._dragging = False
            self.update()
            return
        super(MoveRouteMapView, self).mouseReleaseEvent(event)

    def _appendPathTo(self, target: Tuple[int, int]) -> None:
        if self._currentCell is None or self._mapData is None:
            return
        cx, cy = self._currentCell
        tx, ty = target
        if (cx, cy) == (tx, ty):
            return
        changed = False
        while cx != tx:
            sx = 1 if tx > cx else -1
            cx += sx
            if not self._isInMap(cx, cy):
                break
            self._routeSteps.append((sx, 0))
            self._routeCells.append((cx, cy))
            changed = True
        while cy != ty:
            sy = 1 if ty > cy else -1
            cy += sy
            if not self._isInMap(cx, cy):
                break
            self._routeSteps.append((0, sy))
            self._routeCells.append((cx, cy))
            changed = True
        if changed:
            self._currentCell = (cx, cy)
            self.ROUTE_CHANGED.emit(self.getRoute())
            self.update()

    def _drawOverlay(self, painter: QtGui.QPainter, offset: QtCore.QPoint) -> None:
        self._drawRoute(painter, offset)

    def _drawRoute(self, painter: QtGui.QPainter, offset: QtCore.QPoint) -> None:
        if not self._routeCells:
            return
        painter.save()
        painter.translate(offset)
        painter.setRenderHint(QtGui.QPainter.Antialiasing, True)
        pen = QtGui.QPen(QtGui.QColor(32, 180, 255), max(2, self._tileSize // 8))
        pen.setCapStyle(QtCore.Qt.RoundCap)
        pen.setJoinStyle(QtCore.Qt.RoundJoin)
        painter.setPen(pen)
        points = [self._cellCenter(cell) for cell in self._routeCells]
        for i in range(1, len(points)):
            painter.drawLine(points[i - 1], points[i])
        painter.setPen(QtCore.Qt.NoPen)
        painter.setBrush(QtGui.QColor(255, 215, 0, 220))
        painter.drawEllipse(points[0], max(4, self._tileSize // 5), max(4, self._tileSize // 5))
        if len(points) > 1:
            painter.setBrush(QtGui.QColor(32, 180, 255, 230))
            painter.drawEllipse(points[-1], max(4, self._tileSize // 5), max(4, self._tileSize // 5))
        painter.restore()

    def _cellCenter(self, cell: Tuple[int, int]) -> QtCore.QPointF:
        x, y = cell
        return QtCore.QPointF((x + 0.5) * self._tileSize, (y + 0.5) * self._tileSize)


class MoveRouteEditDialog(QtWidgets.QDialog):
    def __init__(self, value: Any = None, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(MoveRouteEditDialog, self).__init__(parent)
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.Window)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        self.setObjectName("ConfigDictPanel")
        self.setWindowTitle(ELOC("MOVE_ROUTE_EDITOR_TITLE"))
        self.resize(960, 615)
        self.setMinimumSize(825, 540)
        self._setEditorWindowIcon()
        System.SetStyle(self, "config.qss")
        self._mapList = QtWidgets.QListWidget(self)
        self._mapView = MoveRouteMapView(self)
        self._routeLabel = QtWidgets.QLabel(self)
        self._initUI()
        self._loadMaps()
        self._mapView.setRoute(value)
        self._updateRouteLabel(self._mapView.getRoute())

    def getRoute(self) -> List[Tuple[int, int]]:
        return self._mapView.getRoute()

    def _setEditorWindowIcon(self) -> None:
        parent = self.parentWidget()
        icon = parent.windowIcon() if parent is not None else QtGui.QIcon()
        if icon.isNull():
            app = cast(Optional[QtWidgets.QApplication], QtWidgets.QApplication.instance())
            if app is not None:
                icon = app.windowIcon()
        if not icon.isNull():
            self.setWindowIcon(icon)

    def _initUI(self) -> None:
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(12, 12, 12, 12)
        layout.setSpacing(8)
        hint = QtWidgets.QLabel(ELOC("MOVE_ROUTE_EDITOR_HINT"), self)
        hint.setWordWrap(True)
        layout.addWidget(hint)

        splitter = QtWidgets.QSplitter(QtCore.Qt.Horizontal, self)
        self._mapList.setMinimumWidth(180)
        self._mapList.setFrameShape(QtWidgets.QFrame.NoFrame)
        self._mapList.currentItemChanged.connect(self._onMapChanged)
        splitter.addWidget(self._mapList)

        scroll = QtWidgets.QScrollArea(self)
        scroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        scroll.setStyleSheet("QScrollArea { border: none; background: transparent; }")
        scroll.setWidgetResizable(True)
        scroll.setAlignment(QtCore.Qt.AlignCenter)
        scroll.setWidget(self._mapView)
        splitter.addWidget(scroll)
        splitter.setStretchFactor(1, 1)
        layout.addWidget(splitter, 1)

        self._mapView.ROUTE_CHANGED.connect(self._updateRouteLabel)
        layout.addWidget(self._routeLabel)

        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self)
        clearButton = cast(
            QtWidgets.QPushButton, buttons.addButton(ELOC("MOVE_ROUTE_CLEAR"), QtWidgets.QDialogButtonBox.ResetRole)
        )
        clearButton.clicked.connect(self._mapView.clearRoute)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _loadMaps(self) -> None:
        self._mapList.clear()
        localeDict = _LoadGameLocaleDict()
        for key in sorted(GameData.mapData.keys()):
            data = GameData.mapData.get(key)
            if not isinstance(data, dict):
                continue
            resolvedName = _FormatGameString(str(data.get("mapName") or key), localeDict)
            item = QtWidgets.QListWidgetItem(resolvedName)
            item.setData(QtCore.Qt.UserRole, key)
            item.setToolTip(key)
            self._mapList.addItem(item)
        if self._mapList.count() > 0:
            self._mapList.setCurrentRow(0)

    def _onMapChanged(
        self, current: Optional[QtWidgets.QListWidgetItem], previous: Optional[QtWidgets.QListWidgetItem]
    ) -> None:
        if current is None:
            self._mapView.setMapData(None)
            return
        key = current.data(QtCore.Qt.UserRole)
        data = self._buildMapData(str(key))
        self._mapView.setMapData(data)

    def _buildMapData(self, key: str) -> Optional[_RouteMapData]:
        data = GameData.mapData.get(key)
        if not isinstance(data, dict):
            return None
        try:
            localeDict = _LoadGameLocaleDict()
            resolvedName = _FormatGameString(str(data.get("mapName") or key), localeDict)
            return _RouteMapData(
                key=key,
                name=resolvedName if resolvedName != key else key,
                width=int(data.get("width", 0)),
                height=int(data.get("height", 0)),
                layers=data.get("layers", {}) if isinstance(data.get("layers"), dict) else {},
            )
        except (TypeError, ValueError):
            return None

    def _updateRouteLabel(self, route: Any) -> None:
        self._routeLabel.setText(_formatRouteList(route))


class MoveRouteEditor(QtWidgets.QWidget):
    VALUE_CHANGED = QtCore.pyqtSignal(object)

    def __init__(self, value: Any = None, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(MoveRouteEditor, self).__init__(parent)
        self._value = _normaliseRoute(value)
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        self._summary = QtWidgets.QLineEdit(self)
        self._summary.setReadOnly(True)
        self._summary.setFocusPolicy(QtCore.Qt.NoFocus)
        layout.addWidget(self._summary, 1)
        self._button = QtWidgets.QPushButton(ELOC("MOVE_ROUTE_EDIT"), self)
        self._button.clicked.connect(self._openEditor)
        layout.addWidget(self._button)
        self._refreshSummary()

    def getValue(self) -> List[Tuple[int, int]]:
        return [(step[0], step[1]) for step in self._value]

    def setValue(self, value: Any, emit: bool = True) -> None:
        oldValue = self.getValue()
        self._value = _normaliseRoute(value)
        self._refreshSummary()
        if emit and oldValue != self.getValue():
            self.VALUE_CHANGED.emit(self.getValue())

    def setEditable(self, editable: bool) -> None:
        self.setEnabled(editable)
        self._button.setEnabled(editable)

    def _openEditor(self) -> None:
        dlg = MoveRouteEditDialog(self._value, GetIndependentDialogParent(self))
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        if not IsWidgetValid(self):
            return
        self.setValue(dlg.getRoute())

    def _refreshSummary(self) -> None:
        self._summary.setText(_formatRouteList(self._value))


def _LoadGameLocaleDict() -> dict:
    try:
        localeDir = os.path.join(EditorStatus.PROJ_PATH, "Data", "Locale")
        lang = getattr(EditorStatus, "LANGUAGE", "en_GB")
        localeFile = os.path.join(localeDir, lang)
        if not os.path.isfile(localeFile):
            localeFile = os.path.join(localeDir, "en_GB")
        if os.path.isfile(localeFile):
            return File.LoadData(localeFile)
    except Exception:
        pass
    return {}


def _FormatGameString(s: str, localeDict: dict) -> str:
    try:
        return str(s).format(**localeDict)
    except Exception:
        return str(s)


def _normaliseRoute(value: Any) -> List[Tuple[int, int]]:
    if isinstance(value, str):
        value = value.strip()
        if not value:
            return []
        try:
            value = ast.literal_eval(value)
        except (ValueError, SyntaxError):
            return []
    if value is None:
        return []
    result: List[Tuple[int, int]] = []
    if not isinstance(value, (list, tuple)):
        return result
    for item in value:
        step = _normaliseStep(item)
        if step is not None:
            result.append(step)
    return result


def _formatRouteList(route: Any) -> str:
    values = _normaliseRoute(route)
    return str(values)


def _normaliseStep(value: Any) -> Optional[Tuple[int, int]]:
    if hasattr(value, "x") and hasattr(value, "y"):
        try:
            return int(value.x), int(value.y)
        except (TypeError, ValueError):
            return None
    if isinstance(value, (list, tuple)) and len(value) >= 2:
        try:
            return int(value[0]), int(value[1])
        except (TypeError, ValueError):
            return None
    return None
