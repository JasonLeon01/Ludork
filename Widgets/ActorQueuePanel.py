# -*- encoding: utf-8 -*-
from __future__ import annotations
import colorsys
import os
from typing import Any, Optional, Dict
from PyQt5 import QtCore, QtGui, QtWidgets
from EditorGlobal import GameData, EditorStatus


MIN_ICON_SIZE = 24
MAX_ICON_SIZE = 64
ICON_SIZE_PADDING = 24
GRID_SIZE_PADDING = 18
MIN_HEIGHT_EXTRA_PADDING = 2
MIN_PANEL_WIDTH = 180


class ActorQueuePanel(QtWidgets.QWidget):
    SELECTION_CHANGED = QtCore.pyqtSignal(object)
    BLUEPRINT_OPEN_REQUESTED = QtCore.pyqtSignal(str)
    BLUEPRINT_LOCATE_REQUESTED = QtCore.pyqtSignal(str)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None, dockMode: str = "horizontal") -> None:
        super().__init__(parent)
        self._dockMode = ""
        self._queue: list[str] = []
        self._itemMap: Dict[str, QtWidgets.QListWidgetItem] = {}
        self._currentBpRel: Optional[str] = None
        self._pendingClickItem: Optional[QtWidgets.QListWidgetItem] = None
        self._title = QtWidgets.QLabel(ELOC("RECENTLY_PLACED"), self)
        self._list = QtWidgets.QListWidget(self)
        self._list.setViewMode(QtWidgets.QListView.ListMode)
        self._list.setFlow(QtWidgets.QListView.TopToBottom)
        self._list.setWrapping(False)
        self._list.setMovement(QtWidgets.QListView.Static)
        self._list.setResizeMode(QtWidgets.QListView.Fixed)
        self._list.setTextElideMode(QtCore.Qt.ElideMiddle)
        self._list.setSpacing(8)
        self._list.setUniformItemSizes(True)
        self._list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._list.setContextMenuPolicy(QtCore.Qt.CustomContextMenu)
        self._list.customContextMenuRequested.connect(self._showContextMenu)
        self._clickTimer = QtCore.QTimer(self)
        self._clickTimer.setSingleShot(True)
        self._clickTimer.timeout.connect(self._onSingleClickTimeout)
        self._list.itemClicked.connect(self._onItemClicked)
        self._list.itemDoubleClicked.connect(self._onItemDoubleClicked)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self._title)
        layout.addWidget(self._list, 1)
        self.setDockMode(dockMode)
        QtCore.QTimer.singleShot(0, self._updateIconMetrics)

    def resizeEvent(self, event: QtGui.QResizeEvent) -> None:
        super().resizeEvent(event)
        self._updateIconMetrics()

    def setDockMode(self, dockMode: str) -> None:
        m = dockMode.strip().lower() if isinstance(dockMode, str) else "horizontal"
        if m not in ("horizontal", "vertical"):
            m = "horizontal"
        if self._dockMode == m:
            return
        self._dockMode = m
        self._applyDockMode()
        self._updateIconMetrics()

    def _applyDockMode(self) -> None:
        if self._dockMode == "vertical":
            oneColMinWidth = max(MIN_PANEL_WIDTH, self._calculateOneColumnMinWidth())
            self.setMinimumWidth(oneColMinWidth)
            self.setMaximumWidth(16777215)
            self.setMinimumHeight(0)
            self.setMaximumHeight(16777215)
            self.setSizePolicy(QtWidgets.QSizePolicy.Preferred, QtWidgets.QSizePolicy.Expanding)
            return
        oneRowMinHeight = self._calculateOneRowMinHeight()
        self.setMinimumHeight(oneRowMinHeight)
        self.setMaximumHeight(oneRowMinHeight)
        self.setMinimumWidth(0)
        self.setMaximumWidth(16777215)
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Fixed)

    def _calculateOneRowMinHeight(self) -> int:
        fontMetrics = self._list.fontMetrics()
        textHeight = int(fontMetrics.height())
        gridHeight = int(MIN_ICON_SIZE + textHeight + GRID_SIZE_PADDING)
        frameHeight = int(self._list.frameWidth()) * 2
        scrollBar = self._list.horizontalScrollBar()
        scrollBarHeight = int(scrollBar.sizeHint().height()) if scrollBar is not None else 0
        return int(gridHeight + frameHeight + scrollBarHeight + MIN_HEIGHT_EXTRA_PADDING)

    def _calculateOneColumnMinWidth(self) -> int:
        gridWidth = int(MIN_ICON_SIZE + ICON_SIZE_PADDING + GRID_SIZE_PADDING)
        frameWidth = int(self._list.frameWidth()) * 2
        scrollBar = self._list.verticalScrollBar()
        scrollBarWidth = int(scrollBar.sizeHint().width()) if scrollBar is not None else 0
        return int(gridWidth + frameWidth + scrollBarWidth + MIN_HEIGHT_EXTRA_PADDING)

    def _updateIconMetrics(self) -> None:
        iconSize = max(MIN_ICON_SIZE, min(MAX_ICON_SIZE, 48))
        self._list.setIconSize(QtCore.QSize(iconSize, iconSize))

    def clearQueue(self) -> None:
        self._queue.clear()
        self._itemMap.clear()
        self._list.clear()
        self._currentBpRel = None
        self.SELECTION_CHANGED.emit(None)

    def _displayName(self, bpRel: str) -> str:
        if not isinstance(bpRel, str):
            return ""
        s = bpRel.strip()
        if not s:
            return ""
        name = s.split(".")[-1]
        return name

    def _resolveTextureImage(self, texturePath: str | None) -> Optional[QtGui.QImage]:
        path: Optional[str] = None
        if isinstance(texturePath, str) and texturePath.strip():
            p = texturePath.strip()
            if os.path.isabs(p) or p.startswith("Assets/"):
                path = p
            else:
                path = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Characters", p)
        if not path:
            return None
        img = QtGui.QImage(path)
        if img.isNull():
            return None
        return img

    def _getBlueprintAttr(self, bpRel: str | None, attrName: str, default: Any) -> Any:
        if isinstance(bpRel, str):
            prefix = "Data.Blueprints."
            if bpRel.startswith(prefix):
                key = bpRel[len(prefix) :].replace(".", "/")
                bpData = GameData.blueprintsData.get(key)
                if isinstance(bpData, dict):
                    attrs = bpData.get("attrs")
                    if isinstance(attrs, dict) and attrName in attrs:
                        return attrs.get(attrName, default)
        try:
            clsObj = GameData.classDict.get(bpRel, EditorStatus.PROJ_PATH) if isinstance(bpRel, str) else None
            if isinstance(clsObj, type):
                return getattr(clsObj, attrName)
        except AttributeError:
            return default
        except Exception:
            return default
        return default

    def _makePreview(self, bpRel: str) -> QtGui.QPixmap:
        tileSize = getattr(EditorStatus, "CELLSIZE", 32)
        w, h = tileSize, tileSize
        rect = self._getBlueprintAttr(bpRel, "defaultRect", None)
        origin = self._getBlueprintAttr(bpRel, "defaultOrigin", (0.0, 0.0))
        scale = self._getBlueprintAttr(bpRel, "defaultScale", (1.0, 1.0))
        hue = self._normaliseActorHue(self._getBlueprintAttr(bpRel, "hue", 0.0))
        if isinstance(rect, (list, tuple)) and len(rect) >= 2:
            try:
                a, b = rect[0], rect[1]
                if isinstance(a, (list, tuple)) and isinstance(b, (list, tuple)) and len(a) >= 2 and len(b) >= 2:
                    w = int(b[0])
                    h = int(b[1])
            except Exception:
                w, h = tileSize, tileSize
        dw = max(1, int(w * (scale[0] if isinstance(scale, (list, tuple)) and len(scale) >= 2 else 1.0)))
        dh = max(1, int(h * (scale[1] if isinstance(scale, (list, tuple)) and len(scale) >= 2 else 1.0)))
        img = QtGui.QImage(dw, dh, QtGui.QImage.Format_ARGB32)
        img.fill(QtCore.Qt.transparent)
        p = QtGui.QPainter(img)
        p.setRenderHint(QtGui.QPainter.Antialiasing, True)
        texPath = self._getBlueprintAttr(bpRel, "texturePath", "")
        srcImg = self._resolveTextureImage(texPath)
        sx = 0
        sy = 0
        if isinstance(rect, (list, tuple)) and len(rect) >= 2:
            try:
                a, b = rect[0], rect[1]
                if isinstance(a, (list, tuple)) and isinstance(b, (list, tuple)) and len(a) >= 2 and len(b) >= 2:
                    sx = int(a[0])
                    sy = int(a[1])
            except Exception:
                sx, sy = 0, 0
        ox = int(origin[0]) if isinstance(origin, (list, tuple)) and len(origin) >= 2 else 0
        oy = int(origin[1]) if isinstance(origin, (list, tuple)) and len(origin) >= 2 else 0
        if srcImg is not None and isinstance(rect, (list, tuple)) and len(rect) >= 2:
            src = QtCore.QRect(sx, sy, w, h)
            dst = QtCore.QRect(-ox, -oy, dw, dh)
            if self._isNeutralActorHue(hue):
                p.drawImage(dst, srcImg, src)
            else:
                drawImg = self._applyActorHueToImage(srcImg.copy(src), hue)
                p.drawImage(dst, drawImg, QtCore.QRect(0, 0, drawImg.width(), drawImg.height()))
        else:
            color = QtGui.QColor(0, 120, 255, 160)
            r = QtCore.QRect(-ox, -oy, dw, dh)
            p.fillRect(r, color)
            p.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 220), 1))
            p.drawRect(r)
        p.end()
        return QtGui.QPixmap.fromImage(img)

    def _normaliseActorHue(self, hue: Any) -> float:
        try:
            return float(hue) % 360.0
        except (TypeError, ValueError):
            return 0.0

    def _isNeutralActorHue(self, hue: float) -> bool:
        hue = self._normaliseActorHue(hue)
        return hue <= 0.0001 or abs(hue - 360.0) <= 0.0001

    def _applyActorHueToImage(self, image: QtGui.QImage, hue: float) -> QtGui.QImage:
        if image.isNull() or self._isNeutralActorHue(hue):
            return image
        result = image.convertToFormat(QtGui.QImage.Format_ARGB32)
        hueOffset = self._normaliseActorHue(hue) / 360.0
        for y in range(result.height()):
            for x in range(result.width()):
                color = result.pixelColor(x, y)
                if color.alpha() == 0:
                    continue
                h, s, v = colorsys.rgb_to_hsv(color.redF(), color.greenF(), color.blueF())
                r, g, b = colorsys.hsv_to_rgb((h + hueOffset) % 1.0, s, v)
                color.setRgbF(r, g, b, color.alphaF())
                result.setPixelColor(x, y, color)
        return result

    def addOrPromote(self, bpRel: str) -> None:
        b = bpRel.strip() if isinstance(bpRel, str) else ""
        if not b:
            return
        if b in self._queue:
            self._queue.remove(b)
        self._queue.insert(0, b)
        self._currentBpRel = b
        self._rebuildList()
        if b in self._itemMap:
            it = self._itemMap[b]
            self._list.setCurrentItem(it)
            self.SELECTION_CHANGED.emit(b)

    def _rebuildList(self) -> None:
        existingSel: Optional[str] = None
        if isinstance(self._currentBpRel, str) and self._currentBpRel.strip():
            existingSel = self._currentBpRel.strip()
        self._list.clear()
        self._itemMap.clear()
        for bp in self._queue:
            it = QtWidgets.QListWidgetItem()
            it.setData(QtCore.Qt.UserRole, bp)
            it.setText(self._displayName(bp))
            it.setToolTip(bp)
            it.setIcon(QtGui.QIcon(self._makePreview(bp)))
            it.setSizeHint(QtCore.QSize(0, self._list.iconSize().height() + GRID_SIZE_PADDING))
            self._list.addItem(it)
            self._itemMap[bp] = it
        if existingSel and existingSel in self._itemMap:
            it = self._itemMap[existingSel]
            self._list.setCurrentItem(it)

    def purgeStale(self) -> None:
        prefix = "Data.Blueprints."
        stale = [
            bp for bp in self._queue
            if isinstance(bp, str)
            and bp.startswith(prefix)
            and bp[len(prefix):].replace(".", "/") not in GameData.blueprintsData
        ]
        if not stale:
            return
        for bp in stale:
            self._queue.remove(bp)
        if self._currentBpRel in stale:
            self._currentBpRel = None
        self._rebuildList()
        if self._currentBpRel is None:
            self._list.clearSelection()
            self.SELECTION_CHANGED.emit(None)

    def _onItemClicked(self, item: QtWidgets.QListWidgetItem) -> None:
        if item is None:
            return
        bp = item.data(QtCore.Qt.UserRole)
        val = bp.strip() if isinstance(bp, str) else ""
        if val and self._currentBpRel != val:
            self._clickTimer.stop()
            self._pendingClickItem = None
            self._currentBpRel = val
            self._list.setCurrentItem(item)
            self.SELECTION_CHANGED.emit(val)
            return
        self._pendingClickItem = item
        self._clickTimer.start(QtWidgets.QApplication.doubleClickInterval())

    def _onItemDoubleClicked(self, item: QtWidgets.QListWidgetItem) -> None:
        self._clickTimer.stop()
        self._pendingClickItem = None
        bp = item.data(QtCore.Qt.UserRole) if item is not None else None
        val = bp.strip() if isinstance(bp, str) else ""
        if val and self._currentBpRel != val:
            self._currentBpRel = val
            self._list.setCurrentItem(item)
            self.SELECTION_CHANGED.emit(val)
        self._requestOpenBlueprint(item)

    def _onSingleClickTimeout(self) -> None:
        item = self._pendingClickItem
        self._pendingClickItem = None
        if item is None:
            return
        self._toggleSelection(item)

    def _requestOpenBlueprint(self, item: QtWidgets.QListWidgetItem) -> None:
        if item is None:
            return
        bp = item.data(QtCore.Qt.UserRole)
        if isinstance(bp, str) and bp.strip():
            self.BLUEPRINT_OPEN_REQUESTED.emit(bp.strip())

    def _requestLocateBlueprint(self, item: QtWidgets.QListWidgetItem) -> None:
        if item is None:
            return
        bp = item.data(QtCore.Qt.UserRole)
        if isinstance(bp, str) and bp.strip():
            self.BLUEPRINT_LOCATE_REQUESTED.emit(bp.strip())

    def _removeFromQueue(self, bpRel: str) -> None:
        b = bpRel.strip() if isinstance(bpRel, str) else ""
        if not b:
            return
        if b in self._queue:
            self._queue.remove(b)
        self._itemMap.pop(b, None)
        if self._currentBpRel == b:
            self._currentBpRel = None
        self._rebuildList()
        if self._currentBpRel is None:
            self._list.clearSelection()
            self.SELECTION_CHANGED.emit(None)
        else:
            self.SELECTION_CHANGED.emit(self._currentBpRel)

    def _showContextMenu(self, position: QtCore.QPoint) -> None:
        item = self._list.itemAt(position)
        if item is None:
            menu = QtWidgets.QMenu(self)
            from Utils import PluginSystem

            if PluginSystem.AddRightClickActions(menu, self, "actorQueue", "empty", None) > 0:
                menu.exec_(self._list.mapToGlobal(position))
            return
        bp = item.data(QtCore.Qt.UserRole)
        if not isinstance(bp, str) or not bp.strip():
            return
        bpRel = bp.strip()
        previousBpRel = self._currentBpRel
        self._clickTimer.stop()
        self._pendingClickItem = None
        self._currentBpRel = bpRel
        self._list.setCurrentItem(item)
        self.SELECTION_CHANGED.emit(bpRel)
        menu = QtWidgets.QMenu(self)
        actRemove = menu.addAction(ELOC("REMOVE_FROM_RECENTLY_PLACED"))
        actLocate = menu.addAction(ELOC("LOCATE_BLUEPRINT"))
        from Utils import PluginSystem

        PluginSystem.AddRightClickActions(menu, self, "actorQueue", "hit", bpRel)
        action = menu.exec_(self._list.mapToGlobal(position))
        if action == actRemove:
            if previousBpRel != bpRel and previousBpRel in self._queue:
                self._currentBpRel = previousBpRel
            self._removeFromQueue(bpRel)
        elif action == actLocate:
            self._requestLocateBlueprint(item)

    def _toggleSelection(self, item: QtWidgets.QListWidgetItem) -> None:
        if item is None:
            return
        bp = item.data(QtCore.Qt.UserRole)
        if isinstance(bp, str):
            val = bp.strip()
        else:
            val = ""
        if not val:
            self._currentBpRel = None
            self._list.clearSelection()
            self.SELECTION_CHANGED.emit(None)
            return
        if self._currentBpRel == val:
            self._currentBpRel = None
            self._list.clearSelection()
            self.SELECTION_CHANGED.emit(None)
            return
        self._currentBpRel = val
        self._list.setCurrentItem(item)
        self.SELECTION_CHANGED.emit(val)
