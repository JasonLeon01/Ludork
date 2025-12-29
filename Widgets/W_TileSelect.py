# -*- encoding: utf-8 -*-

import os
from typing import Optional, List, Dict
from PyQt5 import QtCore, QtGui, QtWidgets
import EditorStatus
from Data import GameData
from Utils import Panel


class _TileGridView(QtWidgets.QWidget):
    tileClicked = QtCore.pyqtSignal(int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._image: Optional[QtGui.QImage] = None
        self._cell: int = EditorStatus.CELLSIZE
        self._selected: Optional[int] = None
        self.setMouseTracking(True)
        Panel.applyDisabledOpacity(self)

    def setCellSize(self, size: int) -> None:
        if size != self._cell:
            self._cell = size
            self.update()

    def setImage(self, img: Optional[QtGui.QImage]) -> None:
        self._image = img
        self._selected = None
        self._updateSize()
        self.update()

    def setSelected(self, tileNumber: Optional[int]) -> None:
        self._selected = tileNumber
        self.update()

    def clearSelected(self) -> None:
        self.setSelected(None)

    def paintEvent(self, e: QtGui.QPaintEvent) -> None:
        p = QtGui.QPainter(self)
        p.fillRect(self.rect(), QtGui.QColor(30, 30, 30))
        if self._image is None or self._image.isNull():
            p.end()
            return
        p.drawImage(QtCore.QPoint(0, 0), self._image)
        pen = QtGui.QPen(QtGui.QColor(80, 80, 80))
        pen.setWidth(max(1, 1))
        p.setPen(pen)
        w = self._image.width()
        h = self._image.height()
        for x in range(0, w + 1, self._cell):
            p.drawLine(x, 0, x, h)
        for y in range(0, h + 1, self._cell):
            p.drawLine(0, y, w, y)
        if self._selected is not None:
            cols = w // self._cell if self._cell > 0 else 1
            tu = int(self._selected % max(1, cols))
            tv = int(self._selected // max(1, cols))
            r = QtCore.QRect(tu * self._cell, tv * self._cell, self._cell, self._cell)
            selPen = QtGui.QPen(QtWidgets.QApplication.palette().highlight().color())
            selPen.setWidth(max(2, 2))
            p.setPen(selPen)
            p.drawRect(r)
        p.end()

    def _updateSize(self) -> None:
        if self._image is None or self._image.isNull():
            self.setMinimumSize(0, 0)
            return
        self.setMinimumSize(self._image.size())
        self.resize(self._image.size())

    def mousePressEvent(self, e: QtGui.QMouseEvent) -> None:
        if self._image is None or self._image.isNull():
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        if x < 0 or y < 0 or x >= self._image.width() or y >= self._image.height():
            return
        gx = x // self._cell
        gy = y // self._cell
        cols = self._image.width() // self._cell if self._cell > 0 else 1
        tileNumber = gy * max(1, cols) + gx
        self.tileClicked.emit(int(tileNumber))

    def mouseMoveEvent(self, e: QtGui.QMouseEvent) -> None:
        if self._image is None or self._image.isNull():
            return
        if not (e.buttons() & QtCore.Qt.LeftButton):
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        if x < 0 or y < 0 or x >= self._image.width() or y >= self._image.height():
            return
        gx = x // self._cell
        gy = y // self._cell
        cols = self._image.width() // self._cell if self._cell > 0 else 1
        tileNumber = gy * max(1, cols) + gx
        if self._selected != int(tileNumber):
            self.tileClicked.emit(int(tileNumber))


class TileSelect(QtWidgets.QWidget):
    tilesetChanged = QtCore.pyqtSignal(str)
    tileSelected = QtCore.pyqtSignal(int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._allowSelect: bool = False
        self._tilesets: Dict[str, object] = {}
        self._keys: List[str] = []
        self._currentKey: Optional[str] = None
        self._selectedTileNumber: Optional[int] = None
        self._topList = QtWidgets.QListWidget(self)
        self._topList.setFlow(QtWidgets.QListView.LeftToRight)
        self._topList.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self._topList.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self._topList.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._topList.setFixedHeight(40)
        self._grid = _TileGridView(self)
        self._grid.setCellSize(EditorStatus.CELLSIZE)
        self._grid.tileClicked.connect(self._onTileClicked)
        self._scroll = QtWidgets.QScrollArea(self)
        self._scroll.setWidget(self._grid)
        self._scroll.setWidgetResizable(False)
        self._scroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self._scroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self._scroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        layout.addWidget(self._topList, 0)
        layout.addWidget(self._scroll, 1)
        Panel.applyDisabledOpacity(self)
        self._topList.currentRowChanged.connect(self._onTilesetRowChanged)
        self.initTilesets()

    def initTilesets(self) -> None:
        old_key = self._currentKey
        self._tilesets = GameData.tilesetData
        self._keys = list(self._tilesets.keys())
        self._topList.clear()
        for k in self._keys:
            it = QtWidgets.QListWidgetItem(k)
            self._topList.addItem(it)
        if self._keys:
            idx = 0
            if old_key and old_key in self._keys:
                idx = self._keys.index(old_key)
            self._topList.setCurrentRow(idx)

    def setLayerSelected(self, selected: bool) -> None:
        self._allowSelect = bool(selected)
        if not self._allowSelect:
            self.clearSelection()
        Panel.applyDisabledOpacity(self._grid)

    def setCurrentTilesetKey(self, key: Optional[str]) -> None:
        if not key:
            self._topList.clearSelection()
            return
        for i, k in enumerate(self._keys):
            if k == key:
                self._topList.setCurrentRow(i)
                return

    def clearSelection(self) -> None:
        self._selectedTileNumber = None
        self._grid.clearSelected()
        self.tileSelected.emit(-1)

    def setSelectedTileNumber(self, num: Optional[int]) -> None:
        if num is None or int(num) < 0:
            self.clearSelection()
            return
        self._selectedTileNumber = int(num)
        self._grid.setSelected(self._selectedTileNumber)
        self.tileSelected.emit(self._selectedTileNumber)

    def _onTilesetRowChanged(self, idx: int) -> None:
        if idx < 0 or idx >= len(self._keys):
            self._currentKey = None
            self._grid.setImage(None)
            return
        self._currentKey = self._keys[idx]
        ts = self._tilesets.get(self._currentKey)
        if ts is None:
            self._grid.setImage(None)
            return
        p = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets", ts.fileName)
        img = QtGui.QImage(p)
        self._grid.setImage(img if not img.isNull() else None)
        self.tilesetChanged.emit(self._currentKey)

    def _onTileClicked(self, tileNumber: int) -> None:
        if not self._allowSelect:
            self.clearSelection()
            return
        if self._selectedTileNumber == tileNumber:
            self._selectedTileNumber = None
            self._grid.clearSelected()
            self.tileSelected.emit(-1)
            return
        self._selectedTileNumber = tileNumber
        self._grid.setSelected(tileNumber)
        self.tileSelected.emit(tileNumber)
