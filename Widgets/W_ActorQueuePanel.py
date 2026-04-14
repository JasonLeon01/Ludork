# -*- encoding: utf-8 -*-
from __future__ import annotations
import os
from typing import Optional, Dict
from PyQt5 import QtCore, QtGui, QtWidgets
from EditorGlobal import GameData


class ActorQueuePanel(QtWidgets.QWidget):
    selectionChanged = QtCore.pyqtSignal(object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None):
        super().__init__(parent)
        self._queue: list[str] = []
        self._itemMap: Dict[str, QtWidgets.QListWidgetItem] = {}
        self._currentBpRel: Optional[str] = None
        self._list = QtWidgets.QListWidget(self)
        self._list.setViewMode(QtWidgets.QListView.IconMode)
        self._list.setMovement(QtWidgets.QListView.Static)
        self._list.setResizeMode(QtWidgets.QListView.Adjust)
        self._list.setFlow(QtWidgets.QListView.LeftToRight)
        self._list.setSpacing(8)
        self._list.setIconSize(QtCore.QSize(64, 64))
        self._list.setSelectionMode(QtWidgets.QAbstractItemView.SingleSelection)
        self._list.itemClicked.connect(self._onItemClicked)
        layout = QtWidgets.QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        layout.addWidget(self._list, 1)
        self.setMinimumHeight(120)

    def clearQueue(self) -> None:
        self._queue.clear()
        self._itemMap.clear()
        self._list.clear()
        self._currentBpRel = None
        self.selectionChanged.emit(None)

    def _displayName(self, bpRel: str) -> str:
        if not isinstance(bpRel, str):
            return ""
        s = bpRel.strip()
        if not s:
            return ""
        name = s.split(".")[-1]
        return name

    def _resolveTextureImage(self, texturePath: object) -> Optional[QtGui.QImage]:
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

    def _getBlueprintAttr(self, bpRel: object, attrName: str, default: object) -> object:
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
            if clsObj is not None and hasattr(clsObj, attrName):
                return getattr(clsObj, attrName)
        except Exception:
            return default
        return default

    def _makePreview(self, bpRel: str) -> QtGui.QPixmap:
        tileSize = getattr(EditorStatus, "CELLSIZE", 32)
        w, h = tileSize, tileSize
        rect = self._getBlueprintAttr(bpRel, "defaultRect", None)
        origin = self._getBlueprintAttr(bpRel, "defaultOrigin", (0.0, 0.0))
        scale = self._getBlueprintAttr(bpRel, "defaultScale", (1.0, 1.0))
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
            p.drawImage(dst, srcImg, src)
        else:
            color = QtGui.QColor(0, 120, 255, 160)
            r = QtCore.QRect(-ox, -oy, dw, dh)
            p.fillRect(r, color)
            p.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 220), 1))
            p.drawRect(r)
        p.end()
        return QtGui.QPixmap.fromImage(img)

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
            self.selectionChanged.emit(b)

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
            self._list.addItem(it)
            self._itemMap[bp] = it
        if existingSel and existingSel in self._itemMap:
            it = self._itemMap[existingSel]
            self._list.setCurrentItem(it)

    def _onItemClicked(self, item: QtWidgets.QListWidgetItem) -> None:
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
            self.selectionChanged.emit(None)
            return
        if self._currentBpRel == val:
            self._currentBpRel = None
            self._list.clearSelection()
            self.selectionChanged.emit(None)
            return
        self._currentBpRel = val
        self._list.setCurrentItem(item)
        self.selectionChanged.emit(val)
