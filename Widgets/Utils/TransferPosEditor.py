# -*- encoding: utf-8 -*-

from __future__ import annotations

import os
from typing import Any, Callable, Dict, Optional, Tuple, cast

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal import EditorStatus, GameData
from Utils import System
from Utils.DataConfig import DATA_FILE_EXTENSIONS_DAT_FIRST, DATA_FORMAT_DAT, DATA_FORMAT_EXTENSIONS
from .DialogUtils import GetIndependentDialogParent, IsWidgetValid
from ._MapViewBase import MapRenderViewBase
from .MoveRouteEditor import _RouteMapData, _LoadGameLocaleDict, _FormatGameString


def _normaliseMapKey(mapKey: str) -> str:
    path = str(mapKey).replace("\\", "/")
    while path.startswith("./"):
        path = path[2:]
    marker = "Data/Maps/"
    markerIndex = path.find(marker)
    if markerIndex != -1:
        path = path[markerIndex + len(marker) :]
    return os.path.splitext(path)[0]


def _resolveMapFilePath(mapKey: str) -> str:
    path = str(mapKey).replace("\\", "/")
    while path.startswith("./"):
        path = path[2:]
    marker = "Data/Maps/"
    markerIndex = path.find(marker)
    if markerIndex != -1:
        path = path[markerIndex + len(marker) :]
    if os.path.splitext(path)[1]:
        return path
    mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
    for ext in DATA_FILE_EXTENSIONS_DAT_FIRST:
        candidate = f"{path}{ext}"
        if os.path.isfile(os.path.join(mapsRoot, candidate)):
            return candidate
    return f"{path}{DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]}"


class TransferPosMapView(MapRenderViewBase):

    POSITION_CHANGED = QtCore.pyqtSignal(object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(TransferPosMapView, self).__init__(parent)
        self._mapData: Optional[_RouteMapData] = None
        self._selectedCell: Optional[Tuple[int, int]] = None
        self._hoverCell: Optional[Tuple[int, int]] = None

    def setMapData(self, mapData: Optional[_RouteMapData]) -> None:
        self._mapData = mapData
        self._renderMap()
        self._updateContentSize()
        self.update()

    def getPosition(self) -> Optional[Tuple[int, int]]:
        return self._selectedCell

    def setPosition(self, pos: Any) -> None:
        self._selectedCell = _normalisePos(pos)
        self.update()

    def clearPosition(self) -> None:
        self._selectedCell = None
        self.POSITION_CHANGED.emit(None)
        self.update()

    def mousePressEvent(self, event: QtGui.QMouseEvent) -> None:
        if event.button() != QtCore.Qt.LeftButton:
            super(TransferPosMapView, self).mousePressEvent(event)
            return
        cell = self._cellFromPos(event.pos())
        if cell is None:
            return
        self._selectedCell = cell
        self.POSITION_CHANGED.emit(cell)
        self.update()

    def mouseMoveEvent(self, event: QtGui.QMouseEvent) -> None:
        cell = self._cellFromPos(event.pos())
        if cell != self._hoverCell:
            self._hoverCell = cell
            self.update()
        super(TransferPosMapView, self).mouseMoveEvent(event)

    def leaveEvent(self, event: QtCore.QEvent) -> None:
        self._hoverCell = None
        self.update()
        super(TransferPosMapView, self).leaveEvent(event)

    def _drawOverlay(self, painter: QtGui.QPainter, offset: QtCore.QPoint) -> None:
        self._drawHighlights(painter, offset)

    def _drawHighlights(self, painter: QtGui.QPainter, offset: QtCore.QPoint) -> None:
        painter.save()
        painter.translate(offset)
        painter.setRenderHint(QtGui.QPainter.Antialiasing, False)
        if self._hoverCell is not None and self._hoverCell != self._selectedCell:
            hx, hy = self._hoverCell
            painter.fillRect(
                QtCore.QRect(hx * self._tileSize, hy * self._tileSize, self._tileSize, self._tileSize),
                QtGui.QColor(255, 255, 255, 35),
            )
        if self._selectedCell is not None:
            sx, sy = self._selectedCell
            if self._isInMap(sx, sy):
                r = QtCore.QRect(sx * self._tileSize, sy * self._tileSize, self._tileSize, self._tileSize)
                painter.fillRect(r, QtGui.QColor(32, 180, 255, 90))
                painter.setPen(QtGui.QPen(QtGui.QColor(32, 180, 255), 2))
                painter.drawRect(r.adjusted(1, 1, -1, -1))
                cx = sx * self._tileSize + self._tileSize // 2
                cy = sy * self._tileSize + self._tileSize // 2
                half = max(3, self._tileSize // 4)
                painter.setPen(QtGui.QPen(QtGui.QColor(255, 215, 0), 2))
                painter.drawLine(cx - half, cy, cx + half, cy)
                painter.drawLine(cx, cy - half, cx, cy + half)
        painter.restore()


class TransferPosPickDialog(QtWidgets.QDialog):

    def __init__(
        self,
        value: Any = None,
        mapKey: str = "",
        parent: Optional[QtWidgets.QWidget] = None,
    ) -> None:
        super(TransferPosPickDialog, self).__init__(parent)
        self.setWindowFlags(self.windowFlags() | QtCore.Qt.Window)
        self.setAttribute(QtCore.Qt.WA_StyledBackground, True)
        self.setObjectName("ConfigDictPanel")
        self.setWindowTitle(ELOC("TRANSFER_POS_EDITOR_TITLE"))
        self.resize(960, 615)
        self.setMinimumSize(825, 540)
        self._setEditorWindowIcon()
        System.SetStyle(self, "config.qss")
        self._mapList = QtWidgets.QListWidget(self)
        self._mapView = TransferPosMapView(self)
        self._posLabel = QtWidgets.QLabel(self)
        self._initUI()
        self._loadMaps(mapKey)
        self._mapView.setPosition(value)
        self._updatePosLabel(self._mapView.getPosition())

    def getPosition(self) -> Optional[Tuple[int, int]]:
        return self._mapView.getPosition()

    def getMapKey(self) -> str:
        current = self._mapList.currentItem()
        if current is None:
            return ""
        return str(current.data(QtCore.Qt.UserRole))

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
        hint = QtWidgets.QLabel(ELOC("TRANSFER_POS_EDITOR_HINT"), self)
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

        self._mapView.POSITION_CHANGED.connect(self._updatePosLabel)
        layout.addWidget(self._posLabel)

        buttons = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel, self
        )
        ok_btn = buttons.button(QtWidgets.QDialogButtonBox.Ok)
        cancel_btn = buttons.button(QtWidgets.QDialogButtonBox.Cancel)
        if ok_btn:
            ok_btn.setText(ELOC("CONFIRM"))
        if cancel_btn:
            cancel_btn.setText(ELOC("CANCEL"))
        clearButton = cast(
            QtWidgets.QPushButton,
            buttons.addButton(ELOC("TRANSFER_POS_CLEAR"), QtWidgets.QDialogButtonBox.ResetRole),
        )
        clearButton.clicked.connect(self._mapView.clearPosition)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        layout.addWidget(buttons)

    def _loadMaps(self, preferKey: str) -> None:
        self._mapList.clear()
        localeDict = _LoadGameLocaleDict()
        preferRow = -1
        preferMapKey = _normaliseMapKey(preferKey) if preferKey else ""
        for row, key in enumerate(sorted(GameData.mapData.keys())):
            data = GameData.mapData.get(key)
            if not isinstance(data, dict):
                continue
            resolvedName = _FormatGameString(str(data.get("mapName") or key), localeDict)
            item = QtWidgets.QListWidgetItem(resolvedName)
            item.setData(QtCore.Qt.UserRole, key)
            item.setToolTip(key)
            self._mapList.addItem(item)
            if key == preferMapKey:
                preferRow = self._mapList.count() - 1

        if preferRow >= 0:
            self._mapList.setCurrentRow(preferRow)
        elif self._mapList.count() > 0:
            self._mapList.setCurrentRow(0)

    def _onMapChanged(
        self,
        current: Optional[QtWidgets.QListWidgetItem],
        previous: Optional[QtWidgets.QListWidgetItem],
    ) -> None:
        if current is None:
            self._mapView.setMapData(None)
            return
        key = current.data(QtCore.Qt.UserRole)
        self._mapView.setMapData(self._buildMapData(str(key)))

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

    def _updatePosLabel(self, pos: Any) -> None:
        if pos is None:
            self._posLabel.setText(ELOC("TRANSFER_POS_NONE"))
        else:
            self._posLabel.setText(ELOC("TRANSFER_POS_LABEL").format(x=pos[0], y=pos[1]))


class TransferPosEditor(QtWidgets.QWidget):

    VALUE_CHANGED = QtCore.pyqtSignal(object)
    MAP_KEY_CHANGED = QtCore.pyqtSignal(str)

    def __init__(
        self,
        value: Any = None,
        parent: Optional[QtWidgets.QWidget] = None,
    ) -> None:
        super(TransferPosEditor, self).__init__(parent)
        self._value: Optional[Tuple[int, int]] = _normalisePos(value)
        self._mapKeyGetter: Callable[[], str] = lambda: ""
        layout = QtWidgets.QHBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        self._summary = QtWidgets.QLineEdit(self)
        self._summary.setReadOnly(True)
        self._summary.setFocusPolicy(QtCore.Qt.NoFocus)
        layout.addWidget(self._summary, 1)
        self._button = QtWidgets.QPushButton(ELOC("TRANSFER_POS_EDIT"), self)
        self._button.clicked.connect(self._openEditor)
        layout.addWidget(self._button)
        self._refreshSummary()

    def setMapKeyGetter(self, getter: Callable[[], str]) -> None:
        self._mapKeyGetter = getter

    def getValue(self) -> Optional[Tuple[int, int]]:
        return self._value

    def setValue(self, value: Any, emit: bool = True) -> None:
        oldValue = self.getValue()
        self._value = _normalisePos(value)
        self._refreshSummary()
        if emit and oldValue != self.getValue():
            self.VALUE_CHANGED.emit(self.getValue())

    def setEditable(self, editable: bool) -> None:
        self.setEnabled(editable)
        self._button.setEnabled(editable)

    def _openEditor(self) -> None:
        mapKey = self._mapKeyGetter()
        dlg = TransferPosPickDialog(self._value, mapKey, GetIndependentDialogParent(self))
        if dlg.exec_() != QtWidgets.QDialog.Accepted:
            return
        if not IsWidgetValid(self):
            return
        selectedMapKey = dlg.getMapKey()
        self.setValue(dlg.getPosition())
        if not IsWidgetValid(self):
            return
        if selectedMapKey:
            currentMapKey = self._mapKeyGetter()
            if not currentMapKey or not _mapKeyIsValid(currentMapKey):
                self.MAP_KEY_CHANGED.emit(_resolveMapFilePath(selectedMapKey))

    def _refreshSummary(self) -> None:
        if self._value is None:
            self._summary.setText(ELOC("TRANSFER_POS_NONE"))
        else:
            self._summary.setText(ELOC("TRANSFER_POS_LABEL").format(x=self._value[0], y=self._value[1]))


def _normalisePos(value: Any) -> Optional[Tuple[int, int]]:
    if value is None:
        return None
    if isinstance(value, (list, tuple)) and len(value) >= 2:
        try:
            return (int(value[0]), int(value[1]))
        except (TypeError, ValueError):
            return None
    if hasattr(value, "x") and hasattr(value, "y"):
        try:
            return (int(value.x), int(value.y))
        except (TypeError, ValueError):
            return None
    return None


def _mapKeyIsValid(key: str) -> bool:
    mapKey = _normaliseMapKey(key)
    return bool(mapKey) and GameData.mapData.get(mapKey) is not None
