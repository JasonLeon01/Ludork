# -*- encoding: utf-8 -*-

import os
from typing import Optional, List, Dict
from PyQt5 import QtCore, QtGui, QtWidgets
from EditorGlobal import EditorStatus, GameData
from Utils import Panel


class _TileGridView(QtWidgets.QWidget):
    TILE_CLICKED = QtCore.pyqtSignal(int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._image: Optional[QtGui.QImage] = None
        self._cell: int = EditorStatus.CELLSIZE
        self._selected: Optional[int] = None
        self.setMouseTracking(True)
        Panel.ApplyDisabledOpacity(self)

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
        self.TILE_CLICKED.emit(int(tileNumber))

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
            self.TILE_CLICKED.emit(int(tileNumber))


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
    AUTOTILE_SELECTED = QtCore.pyqtSignal(str)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super().__init__(parent)
        self._allowSelect: bool = False
        self._tilesets: Dict[str, object] = {}
        self._keys: List[str] = []
        self._currentKey: Optional[str] = None
        self._selectedTileNumber: Optional[int] = None
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
        if self._selectedAutoTileKey is not None:
            self._selectedAutoTileKey = None
            self._autoTileBar.setSelectedKey(None)
            self.AUTOTILE_SELECTED.emit("")

    def setSelectedTileNumber(self, num: Optional[int]) -> None:
        if num is None or int(num) < 0:
            self.clearSelection()
            return
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
        if self._selectedTileNumber == tileNumber and self._selectedAutoTileKey is None:
            self._selectedTileNumber = None
            self._grid.clearSelected()
            self.TILE_SELECTED.emit(-1)
            return
        if self._selectedAutoTileKey is not None:
            self._selectedAutoTileKey = None
            self._autoTileBar.setSelectedKey(None)
            self.AUTOTILE_SELECTED.emit("")
        self._selectedTileNumber = tileNumber
        self._grid.setSelected(tileNumber)
        self.TILE_SELECTED.emit(tileNumber)

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
        self._selectedAutoTileKey = key
        self._autoTileBar.setSelectedKey(key)
        self.AUTOTILE_SELECTED.emit(key)
