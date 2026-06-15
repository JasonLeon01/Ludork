# -*- encoding: utf-8 -*-

import os
from typing import Optional, List, Dict, Tuple
from PyQt5 import QtCore, QtGui, QtWidgets
from EditorGlobal import EditorStatus, GameData
from Utils import Panel


class _TileGridView(QtWidgets.QWidget):
    TILE_CLICKED = QtCore.pyqtSignal(int)
    TILE_PATTERN_SELECTED = QtCore.pyqtSignal(int, int, int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._image: Optional[QtGui.QImage] = None
        self._cell: int = EditorStatus.CELLSIZE
        self._selected: Optional[int] = None
        self._selectedRect: Optional[QtCore.QRect] = None
        self._dragStartCell: Optional[Tuple[int, int]] = None
        self._lastEmittedRect: Optional[QtCore.QRect] = None
        self.setMouseTracking(True)
        Panel.ApplyDisabledOpacity(self)

    def setCellSize(self, size: int) -> None:
        if size != self._cell:
            self._cell = size
            self.update()

    def setImage(self, img: Optional[QtGui.QImage]) -> None:
        self._image = img
        self._selected = None
        self._selectedRect = None
        self._dragStartCell = None
        self._lastEmittedRect = None
        self._updateSize()
        self.update()

    def setSelected(self, tileNumber: Optional[int]) -> None:
        self._selected = tileNumber
        self._selectedRect = None
        if tileNumber is not None and self._image is not None and not self._image.isNull():
            cols = self._image.width() // self._cell if self._cell > 0 else 1
            tu = int(tileNumber % max(1, cols))
            tv = int(tileNumber // max(1, cols))
            self._selectedRect = QtCore.QRect(tu, tv, 1, 1)
        self.update()

    def setSelectedPattern(self, originTileNumber: int, width: int, height: int) -> None:
        self._selected = None
        self._selectedRect = None
        if self._image is not None and not self._image.isNull():
            cols = self._image.width() // self._cell if self._cell > 0 else 1
            tu = int(originTileNumber % max(1, cols))
            tv = int(originTileNumber // max(1, cols))
            self._selectedRect = QtCore.QRect(tu, tv, max(1, int(width)), max(1, int(height)))
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
        if self._selectedRect is not None:
            r = QtCore.QRect(
                self._selectedRect.x() * self._cell,
                self._selectedRect.y() * self._cell,
                self._selectedRect.width() * self._cell,
                self._selectedRect.height() * self._cell,
            )
            selPen = QtGui.QPen(QtWidgets.QApplication.palette().highlight().color())
            selPen.setWidth(max(2, 2))
            p.setPen(selPen)
            p.drawRect(r.adjusted(0, 0, -1, -1))
        p.end()

    def _updateSize(self) -> None:
        if self._image is None or self._image.isNull():
            self.setMinimumSize(0, 0)
            return
        self.setMinimumSize(self._image.size())
        self.resize(self._image.size())

    def _cellAt(self, pos: QtCore.QPoint) -> Optional[Tuple[int, int]]:
        if self._image is None or self._image.isNull():
            return None
        x = int(pos.x())
        y = int(pos.y())
        if x < 0 or y < 0 or x >= self._image.width() or y >= self._image.height():
            return None
        gx = x // self._cell
        gy = y // self._cell
        return gx, gy

    def _rectFromCells(self, a: Tuple[int, int], b: Tuple[int, int]) -> QtCore.QRect:
        ax, ay = a
        bx, by = b
        min_x, max_x = min(ax, bx), max(ax, bx)
        min_y, max_y = min(ay, by), max(ay, by)
        return QtCore.QRect(min_x, min_y, max_x - min_x + 1, max_y - min_y + 1)

    def _emitRect(self, rect: QtCore.QRect) -> None:
        if self._image is None or self._image.isNull():
            return
        if self._lastEmittedRect == rect:
            return
        cols = self._image.width() // self._cell if self._cell > 0 else 1
        tileNumber = rect.y() * max(1, cols) + rect.x()
        self._lastEmittedRect = QtCore.QRect(rect)
        if rect.width() == 1 and rect.height() == 1:
            self.TILE_CLICKED.emit(int(tileNumber))
        else:
            self.TILE_PATTERN_SELECTED.emit(int(tileNumber), int(rect.width()), int(rect.height()))

    def mousePressEvent(self, e: QtGui.QMouseEvent) -> None:
        if e.button() != QtCore.Qt.LeftButton:
            return
        cell = self._cellAt(e.pos())
        if cell is None:
            return
        self._dragStartCell = cell
        rect = self._rectFromCells(cell, cell)
        self._emitRect(rect)

    def mouseMoveEvent(self, e: QtGui.QMouseEvent) -> None:
        if not (e.buttons() & QtCore.Qt.LeftButton):
            return
        cell = self._cellAt(e.pos())
        if cell is None:
            return
        start = self._dragStartCell if self._dragStartCell is not None else cell
        rect = self._rectFromCells(start, cell)
        self._emitRect(rect)

    def mouseReleaseEvent(self, e: QtGui.QMouseEvent) -> None:
        if e.button() == QtCore.Qt.LeftButton:
            self._dragStartCell = None
            self._lastEmittedRect = None
        super().mouseReleaseEvent(e)


class _AutoTileBar(QtWidgets.QListWidget):
    AUTOTILE_CLICKED = QtCore.pyqtSignal(str)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._cell: int = EditorStatus.CELLSIZE
        self._keys: List[str] = []
        self._selectedKey: Optional[str] = None
        self.setFlow(QtWidgets.QListView.LeftToRight)
        self.setWrapping(False)
        self.setMovement(QtWidgets.QListView.Static)
        self.setResizeMode(QtWidgets.QListView.Adjust)
        self.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAlwaysOff)
        self.setSelectionMode(QtWidgets.QAbstractItemView.NoSelection)
        self.setSpacing(2)
        self.setUniformItemSizes(True)
        self.setIconSize(QtCore.QSize(self._cell, self._cell))
        self.setViewMode(QtWidgets.QListView.IconMode)
        self.setFrameShape(QtWidgets.QFrame.NoFrame)
        h = self._cell + 16
        self.setFixedHeight(h)
        self.itemClicked.connect(self._onItemClicked)
        Panel.ApplyDisabledOpacity(self)

    def setCellSize(self, size: int) -> None:
        if size != self._cell:
            self._cell = size
            self.setIconSize(QtCore.QSize(self._cell, self._cell))
            self.setFixedHeight(self._cell + 16)
            self.refresh()

    def refresh(self) -> None:
        self.blockSignals(True)
        self.clear()
        self._keys = list(GameData.autoTileData.keys())
        for key in self._keys:
            data = GameData.autoTileData.get(key)
            icon = self._buildIcon(data, highlighted=(key == self._selectedKey))
            item = QtWidgets.QListWidgetItem(icon, "")
            item.setToolTip(key)
            item.setData(QtCore.Qt.UserRole, key)
            item.setSizeHint(QtCore.QSize(self._cell + 4, self._cell + 4))
            self.addItem(item)
        self.blockSignals(False)

    def setSelectedKey(self, key: Optional[str]) -> None:
        if self._selectedKey == key:
            return
        self._selectedKey = key
        self.refresh()

    def selectedKey(self) -> Optional[str]:
        return self._selectedKey

    def _buildIcon(self, data, highlighted: bool = False) -> QtGui.QIcon:
        size = self._cell
        pix = QtGui.QPixmap(size, size)
        pix.fill(QtGui.QColor(60, 60, 60))
        if data is not None:
            fileName = getattr(data, "fileName", "") or ""
            if fileName:
                path = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Autotiles", fileName)
                if os.path.exists(path):
                    img = QtGui.QImage(path)
                    if not img.isNull():
                        rect = QtCore.QRect(0, 0, min(size, img.width()), min(size, img.height()))
                        crop = img.copy(rect)
                        if crop.size() != QtCore.QSize(size, size):
                            crop = crop.scaled(
                                size, size, QtCore.Qt.KeepAspectRatio, QtCore.Qt.FastTransformation
                            )
                        pix = QtGui.QPixmap.fromImage(crop)
        if highlighted:
            painter = QtGui.QPainter(pix)
            selColor = QtWidgets.QApplication.palette().highlight().color()
            pen = QtGui.QPen(selColor)
            pen.setWidth(2)
            painter.setPen(pen)
            painter.drawRect(1, 1, size - 2, size - 2)
            painter.end()
        return QtGui.QIcon(pix)

    def _onItemClicked(self, item: QtWidgets.QListWidgetItem) -> None:
        key = item.data(QtCore.Qt.UserRole)
        if isinstance(key, str):
            self.AUTOTILE_CLICKED.emit(key)


class TileSelect(QtWidgets.QWidget):
    TILESET_CHANGED = QtCore.pyqtSignal(str)
    TILE_SELECTED = QtCore.pyqtSignal(int)
    TILE_PATTERN_SELECTED = QtCore.pyqtSignal(list)
    AUTOTILE_SELECTED = QtCore.pyqtSignal(str)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._allowSelect: bool = False
        self._tilesets: Dict[str, object] = {}
        self._keys: List[str] = []
        self._currentKey: Optional[str] = None
        self._selectedTileNumber: Optional[int] = None
        self._selectedTilePattern: Optional[List[List[int]]] = None
        self._selectedAutoTileKey: Optional[str] = None
        self._autoTileBar = _AutoTileBar(self)
        self._autoTileBar.AUTOTILE_CLICKED.connect(self._onAutoTileClicked)
        self._topTabs = QtWidgets.QTabBar(self)
        self._topTabs.setExpanding(False)
        self._topTabs.setMovable(False)
        self._topTabs.setDrawBase(False)
        self._topTabs.setElideMode(QtCore.Qt.ElideRight)
        self._topTabs.setFocusPolicy(QtCore.Qt.NoFocus)
        self._topTabs.setUsesScrollButtons(True)
        self._topTabs.setFixedHeight(36)
        self._grid = _TileGridView(self)
        self._grid.setCellSize(EditorStatus.CELLSIZE)
        self._grid.TILE_CLICKED.connect(self._onTileClicked)
        self._grid.TILE_PATTERN_SELECTED.connect(self._onTilePatternSelected)
        self._scroll = QtWidgets.QScrollArea(self)
        self._scroll.setWidget(self._grid)
        self._scroll.setWidgetResizable(False)
        self._scroll.setHorizontalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self._scroll.setVerticalScrollBarPolicy(QtCore.Qt.ScrollBarAsNeeded)
        self._scroll.setFrameShape(QtWidgets.QFrame.NoFrame)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(4)
        layout.addWidget(self._autoTileBar, 0)
        layout.addWidget(self._topTabs, 0)
        layout.addWidget(self._scroll, 1)
        Panel.ApplyDisabledOpacity(self)
        self._topTabs.currentChanged.connect(self._onTilesetRowChanged)
        self.initTilesets()
        self.initAutoTiles()

    def initTilesets(self) -> None:
        old_key = self._currentKey
        self._tilesets = GameData.tilesetData
        self._keys = list(self._tilesets.keys())
        self._topTabs.blockSignals(True)
        while self._topTabs.count() > 0:
            self._topTabs.removeTab(0)
        for k in self._keys:
            self._topTabs.addTab(k)
        self._topTabs.blockSignals(False)
        if self._keys:
            idx = 0
            if old_key and old_key in self._keys:
                idx = self._keys.index(old_key)
            self._topTabs.setCurrentIndex(idx)
            self._onTilesetRowChanged(self._topTabs.currentIndex())

    def initAutoTiles(self) -> None:
        if self._selectedAutoTileKey is not None and self._selectedAutoTileKey not in GameData.autoTileData:
            self._selectedAutoTileKey = None
            self.AUTOTILE_SELECTED.emit("")
        self._autoTileBar.setSelectedKey(self._selectedAutoTileKey)
        self._autoTileBar.refresh()

    def setLayerSelected(self, selected: bool) -> None:
        self._allowSelect = bool(selected)
        if not self._allowSelect:
            self.clearSelection()
        Panel.ApplyDisabledOpacity(self._grid)
        Panel.ApplyDisabledOpacity(self._autoTileBar)

    def setCurrentTilesetKey(self, key: Optional[str]) -> None:
        if not key:
            if self._topTabs.count() > 0:
                self._topTabs.setCurrentIndex(-1)
            return
        for i, k in enumerate(self._keys):
            if k == key:
                self._topTabs.setCurrentIndex(i)
                return

    def clearSelection(self) -> None:
        if self._selectedTileNumber is not None:
            self._selectedTileNumber = None
            self._grid.clearSelected()
            self.TILE_SELECTED.emit(-1)
        if self._selectedTilePattern is not None:
            self._selectedTilePattern = None
            self._grid.clearSelected()
            self.TILE_PATTERN_SELECTED.emit([])
        if self._selectedAutoTileKey is not None:
            self._selectedAutoTileKey = None
            self._autoTileBar.setSelectedKey(None)
            self.AUTOTILE_SELECTED.emit("")

    def setSelectedTileNumber(self, num: Optional[int]) -> None:
        if num is None or int(num) < 0:
            self.clearSelection()
            return
        if self._selectedTilePattern is not None:
            self._selectedTilePattern = None
            self.TILE_PATTERN_SELECTED.emit([])
        if self._selectedAutoTileKey is not None:
            self._selectedAutoTileKey = None
            self._autoTileBar.setSelectedKey(None)
            self.AUTOTILE_SELECTED.emit("")
        self._selectedTileNumber = int(num)
        self._grid.setSelected(self._selectedTileNumber)
        self.TILE_SELECTED.emit(self._selectedTileNumber)

    def setSelectedAutoTileKey(self, key: Optional[str]) -> None:
        if key is None or not isinstance(key, str) or key == "":
            if self._selectedAutoTileKey is not None:
                self._selectedAutoTileKey = None
                self._autoTileBar.setSelectedKey(None)
                self.AUTOTILE_SELECTED.emit("")
            return
        if key not in GameData.autoTileData:
            return
        if self._selectedTileNumber is not None:
            self._selectedTileNumber = None
            self._grid.clearSelected()
            self.TILE_SELECTED.emit(-1)
        if self._selectedTilePattern is not None:
            self._selectedTilePattern = None
            self._grid.clearSelected()
            self.TILE_PATTERN_SELECTED.emit([])
        self._selectedAutoTileKey = key
        self._autoTileBar.setSelectedKey(key)
        self.AUTOTILE_SELECTED.emit(key)

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
        self.TILESET_CHANGED.emit(self._currentKey)

    def _onTileClicked(self, tileNumber: int) -> None:
        if not self._allowSelect:
            self.clearSelection()
            return
        if (
            self._selectedTileNumber == tileNumber
            and self._selectedTilePattern is None
            and self._selectedAutoTileKey is None
        ):
            self._selectedTileNumber = None
            self._grid.clearSelected()
            self.TILE_SELECTED.emit(-1)
            return
        if self._selectedTilePattern is not None:
            self._selectedTilePattern = None
            self.TILE_PATTERN_SELECTED.emit([])
        if self._selectedAutoTileKey is not None:
            self._selectedAutoTileKey = None
            self._autoTileBar.setSelectedKey(None)
            self.AUTOTILE_SELECTED.emit("")
        self._selectedTileNumber = tileNumber
        self._grid.setSelected(tileNumber)
        self.TILE_SELECTED.emit(tileNumber)

    def _onTilePatternSelected(self, originTileNumber: int, width: int, height: int) -> None:
        if not self._allowSelect:
            self.clearSelection()
            return
        pattern = self._buildTilePattern(originTileNumber, width, height)
        if not pattern:
            return
        if self._selectedTileNumber is not None:
            self._selectedTileNumber = None
            self.TILE_SELECTED.emit(-1)
        if self._selectedAutoTileKey is not None:
            self._selectedAutoTileKey = None
            self._autoTileBar.setSelectedKey(None)
            self.AUTOTILE_SELECTED.emit("")
        self._selectedTilePattern = pattern
        self._grid.setSelectedPattern(originTileNumber, width, height)
        self.TILE_PATTERN_SELECTED.emit(pattern)

    def _buildTilePattern(self, originTileNumber: int, width: int, height: int) -> List[List[int]]:
        img = self._grid._image
        if img is None or img.isNull():
            return []
        cell = max(1, int(EditorStatus.CELLSIZE))
        cols = max(1, img.width() // cell)
        rows = max(1, img.height() // cell)
        start_x = int(originTileNumber) % cols
        start_y = int(originTileNumber) // cols
        width = max(1, min(int(width), cols - start_x))
        height = max(1, min(int(height), rows - start_y))
        result: List[List[int]] = []
        for y in range(height):
            row: List[int] = []
            for x in range(width):
                row.append((start_y + y) * cols + start_x + x)
            result.append(row)
        return result

    def _onAutoTileClicked(self, key: str) -> None:
        if not self._allowSelect:
            self.clearSelection()
            return
        if self._selectedAutoTileKey == key and self._selectedTileNumber is None:
            self._selectedAutoTileKey = None
            self._autoTileBar.setSelectedKey(None)
            self.AUTOTILE_SELECTED.emit("")
            return
        if self._selectedTileNumber is not None:
            self._selectedTileNumber = None
            self._grid.clearSelected()
            self.TILE_SELECTED.emit(-1)
        if self._selectedTilePattern is not None:
            self._selectedTilePattern = None
            self._grid.clearSelected()
            self.TILE_PATTERN_SELECTED.emit([])
        self._selectedAutoTileKey = key
        self._autoTileBar.setSelectedKey(key)
        self.AUTOTILE_SELECTED.emit(key)
