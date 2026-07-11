# -*- encoding: utf-8 -*-

from __future__ import annotations
import os
import copy
import colorsys
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, Tuple, cast
from PyQt5 import QtWidgets, QtGui, QtCore
import Utils
from EditorGlobal import EditorStatus, GameData
from Utils import EditorData, System, SFMLRender
from Utils.DataConfig import DATA_FILE_EXTENSIONS, DATA_FORMAT_DAT, DATA_FORMAT_EXTENSIONS, DATA_FORMAT_JSON
from .Utils import AutoTileRenderer, TilemapRenderer, Toast


_EDITOR_ANIMATION_TICK_MS = 100
_AUTOTILE_ANIMATION_INTERVAL_MS = 500
_TILE_BRUSH_RENDER_INTERVAL_MS = 16
_DEFAULT_ACTOR_SWITCH_INTERVAL = 0.2
_CHARACTER_SHEET_COLS = 4
_CHARACTER_SHEET_ROWS = 4
_MAP_TILE_SIZE_MIN = 8
_MAP_TILE_SIZE_MAX = 128
_MAP_TILE_SIZE_STEP = 4
GridCell = Optional[int | str]
Grid = List[List[GridCell]]


@dataclass
class MapData:
    mapName: str
    width: int
    height: int
    layers: Dict[str, Any]


@dataclass
class EditorLayerData:
    layerName: str
    layerTilesetKey: str
    tiles: List[List[Optional[int]]]
    autoTiles: List[List[Optional[str]]]
    shaderPath: str = ""
    visible: bool = True


class EditorPanel(QtWidgets.QWidget):
    TILE_NUMBER_PICKED = QtCore.pyqtSignal(int)
    AUTOTILE_PICKED = QtCore.pyqtSignal(str)
    DATA_CHANGED = QtCore.pyqtSignal()
    LIGHT_SELECTION_CHANGED = QtCore.pyqtSignal(str, object, object)
    LIGHT_DATA_CHANGED = QtCore.pyqtSignal(str, object, object)
    ACTOR_SELECTION_CHANGED = QtCore.pyqtSignal(str, object, object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        self.selectedPos: Tuple[int, int] = None
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.mapFilePath = ""
        self.mapKey: str = ""
        self.mapData: Optional[MapData] = None
        self._pixmap: Optional[QtGui.QPixmap] = None
        self._cachedTilesetImage: Optional[QtGui.QImage] = None
        self._actorShaderImageCache: Dict[Tuple[str, str, Tuple[int, int, int, int], int, int], QtGui.QImage] = {}
        self.selectedLayerName: Optional[str] = None
        self.selectedTileNumber: Optional[int] = None
        self.selectedTilePattern: Optional[List[List[int]]] = None
        self.selectedAutoTileKey: Optional[str] = None
        self.tileModeEnabled: bool = True
        self.rectStartPos: Optional[Tuple[int, int]] = None
        self.selectedLightIndex: Optional[int] = None
        self.selectedActorLayerName: Optional[str] = None
        self.selectedActorIndex: Optional[int] = None
        self._lightRadiusDragging = False
        self._lightRadiusDragMapKey = ""
        self._lightRadiusDragIndex: Optional[int] = None
        self._lightRadiusDragCenter: Optional[Tuple[float, float]] = None
        self._lightRadiusDragLastRadius: Optional[float] = None
        self._lightRadiusDragTitleRefreshed = False
        self._lightMoveDragging = False
        self._lightMoveDragMapKey = ""
        self._lightMoveDragIndex: Optional[int] = None
        self._lightMoveDragOffset: Optional[Tuple[float, float]] = None
        self._lightMoveDragTitleRefreshed = False
        self._lightOverlayEnabled = False
        self._actorMoveDragging = False
        self._actorMoveDragLayerName: Optional[str] = None
        self._actorMoveDragIndex: Optional[int] = None
        self._actorMoveDragLastGrid: Optional[Tuple[int, int]] = None
        self._actorMoveDragTitleRefreshed = False
        self._actorClipboard = None
        self._actorClassVarChangesClipboard: Optional[Dict[str, Any]] = None
        self._pendingActorBpRel: Optional[str] = None
        self._autoTileRenderer = AutoTileRenderer()
        self._tilemapRenderer = TilemapRenderer(self._autoTileRenderer)
        self._autoTileFrame: int = 0
        self._actorAnimationTime: float = 0.0
        self._tileSize = EditorStatus.CELLSIZE
        self._hoverGridPos: Optional[Tuple[int, int]] = None
        self._hudLabel: Optional[QtWidgets.QLabel] = None
        self._tileBrushDragging = False
        self._brushBaseImage: Optional[QtGui.QImage] = None
        self._brushLayerImage: Optional[QtGui.QImage] = None
        self._tileBrushRenderPending = False
        super().__init__(parent)
        self.setMouseTracking(True)
        self.setFocusPolicy(QtCore.Qt.ClickFocus)
        self.setAcceptDrops(False)
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)
        Utils.Panel.ApplyDisabledOpacity(self)
        self._animationClock = QtCore.QElapsedTimer()
        self._animationClock.start()
        self._animationTimer = QtCore.QTimer(self)
        self._animationTimer.setInterval(_EDITOR_ANIMATION_TICK_MS)
        self._animationTimer.timeout.connect(self._onAnimationTick)
        self._animationTimer.start()
        self._tileBrushRenderTimer = QtCore.QTimer(self)
        self._tileBrushRenderTimer.setSingleShot(True)
        self._tileBrushRenderTimer.setInterval(_TILE_BRUSH_RENDER_INTERVAL_MS)
        self._tileBrushRenderTimer.timeout.connect(self._onTileBrushRenderTick)
        self._actorShortcuts = [
            QtWidgets.QShortcut(
                QtGui.QKeySequence.Copy,
                self,
                self.copySelectedActor,
                context=QtCore.Qt.WidgetWithChildrenShortcut,
            ),
            QtWidgets.QShortcut(
                QtGui.QKeySequence.Paste,
                self,
                self.pasteActorAtSelectedPos,
                context=QtCore.Qt.WidgetWithChildrenShortcut,
            ),
            QtWidgets.QShortcut(
                QtGui.QKeySequence.Delete,
                self,
                self.deleteSelectedActor,
                context=QtCore.Qt.WidgetWithChildrenShortcut,
            ),
        ]

    def _updateCachedTileset(self) -> None:
        self._cachedTilesetImage = None
        if self.mapData is None or self.selectedLayerName is None:
            return
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        fileName = EditorData.TilesetFileName(GameData.tilesetData.get(layer.layerTilesetKey))
        if not fileName:
            return
        ts_path = os.path.join(
            EditorStatus.PROJ_PATH,
            "Assets",
            "Tilesets",
            fileName,
        )
        if os.path.exists(ts_path):
            self._cachedTilesetImage = QtGui.QImage(ts_path)

    def refreshMap(self, mapFileName: Optional[str] = None):
        self.selectedPos = None
        self._hoverGridPos = None
        self._updateGridHud()
        self.mapData = None
        self._pixmap = None
        self._setSelectedLightIndex(None)
        self._setSelectedActor(None, None)
        self.setMinimumSize(0, 0)
        self.setMaximumSize(16777215, 16777215)
        self.updateGeometry()
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.mapFilePath = ""
        self.mapKey = ""
        Utils.Panel.ClearPanel(self)
        if not mapFileName:
            return
        if os.path.isabs(mapFileName):
            self.mapFilePath = mapFileName
            self.mapKey = os.path.basename(mapFileName)
        else:
            self.mapFilePath = os.path.join(self._mapFilesRoot, mapFileName)
            self.mapKey = mapFileName
        mapData = GameData.mapData.get(self.mapKey)
        if mapData is None:
            mapData = Utils.File.LoadData(self.mapFilePath)
            GameData.mapData[self.mapKey] = mapData
        if isinstance(mapData, dict):
            GameData.CleanMapActorInstanceTransformData(mapData)
        self.applyMapData(mapData)
        self._updateCachedTileset()
        self._renderFromMapData()
        self._updateContentSize()
        self.update()

    def applyMapData(self, data):
        mapName = data["mapName"]
        width = data["width"]
        height = data["height"]
        layers = data["layers"]
        mapLayers = {}
        for layerName, layerData in layers.items():
            name = layerData["layerName"]
            layerTilesetKey = str(layerData.get("layerTileset", ""))
            layerTiles = layerData["tiles"]
            tiles: List[List[Optional[int]]] = []
            for y in range(height):
                tiles.append([])
                for x in range(width):
                    tiles[-1].append(layerTiles[y][x])
            rawAuto = layerData.get("autoTiles")
            autoTiles: List[List[Optional[str]]] = []
            for y in range(height):
                row: List[Optional[str]] = []
                for x in range(width):
                    val = None
                    if (
                        isinstance(rawAuto, list)
                        and y < len(rawAuto)
                        and isinstance(rawAuto[y], list)
                        and x < len(rawAuto[y])
                    ):
                        cell = rawAuto[y][x]
                        if isinstance(cell, str) and cell:
                            val = cell
                    row.append(val)
                autoTiles.append(row)
            if not isinstance(layerData.get("autoTiles"), list):
                layerData["autoTiles"] = [list(r) for r in autoTiles]
            layer = EditorLayerData(
                name,
                layerTilesetKey,
                tiles,
                autoTiles,
                str(layerData.get("shaderPath", "") or ""),
            )
            mapLayers[layerName] = layer
        self.mapData = MapData(mapName, width, height, mapLayers)

    def _buildMapImage(self, skipLayerName: Optional[str] = None) -> Optional[QtGui.QImage]:
        if self.mapData is None:
            return None
        sourceTileSize = EditorStatus.CELLSIZE
        tileSize = self._getTileSize()
        w = self.mapData.width * tileSize
        h = self.mapData.height * tileSize
        img = QtGui.QImage(w, h, QtGui.QImage.Format_ARGB32)
        img.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(img)
        sel = self.selectedLayerName
        for layerName, layer in self.mapData.layers.items():
            if not layer.visible:
                continue
            if layerName == skipLayerName:
                continue
            layerOpacity = 1.0 if (sel is None or layerName == sel) else 0.5
            painter.setOpacity(layerOpacity)
            layerImg = self._tilemapRenderer.renderLayer(
                self.mapData.width,
                self.mapData.height,
                tileSize,
                layer.tiles,
                layer.layerTilesetKey,
                layer.autoTiles,
                self._autoTileFrame,
                sourceTileSize,
            )
            if layerImg is not None and not layerImg.isNull():
                painter.drawImage(0, 0, layerImg)
            self._drawActorsForLayer(painter, layerName, tileSize, layerOpacity)
        painter.end()
        return img

    def _renderFromMapData(self) -> None:
        self._brushBaseImage = None
        self._brushLayerImage = None
        img = self._buildMapImage()
        if img is None:
            self._pixmap = None
            return
        self._pixmap = QtGui.QPixmap.fromImage(img)
        self.update()

    def _renderFromMapDataDuringBrush(self) -> None:
        self._brushLayerImage = None
        if self.mapData is None:
            self._pixmap = None
            return
        layerName = self.selectedLayerName
        if layerName is None:
            self._renderFromMapData()
            return
        layer = self.mapData.layers.get(layerName)
        if layer is None or not layer.visible:
            self._renderFromMapData()
            return
        if self._brushBaseImage is None:
            self._brushBaseImage = self._buildMapImage(skipLayerName=layerName)
            if self._brushBaseImage is None:
                self._renderFromMapData()
                return
        sourceTileSize = EditorStatus.CELLSIZE
        tileSize = self._getTileSize()
        w = self.mapData.width * tileSize
        h = self.mapData.height * tileSize
        img = QtGui.QImage(w, h, QtGui.QImage.Format_ARGB32)
        img.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(img)
        painter.drawImage(0, 0, self._brushBaseImage)
        painter.setOpacity(1.0)
        layerImg = self._tilemapRenderer.renderLayer(
            self.mapData.width,
            self.mapData.height,
            tileSize,
            layer.tiles,
            layer.layerTilesetKey,
            layer.autoTiles,
            self._autoTileFrame,
            sourceTileSize,
        )
        if layerImg is not None and not layerImg.isNull():
            painter.drawImage(0, 0, layerImg)
        self._drawActorsForLayer(painter, layerName, tileSize, 1.0)
        painter.end()
        self._pixmap = QtGui.QPixmap.fromImage(img)
        self.update()

    def _scheduleTileBrushRender(self) -> None:
        self._tileBrushRenderPending = True
        if not self._tileBrushRenderTimer.isActive():
            self._tileBrushRenderTimer.start(_TILE_BRUSH_RENDER_INTERVAL_MS)

    def _onTileBrushRenderTick(self) -> None:
        if not self._tileBrushRenderPending:
            return
        self._tileBrushRenderPending = False
        if self._tileBrushDragging:
            self._renderFromMapDataDuringBrush()
        else:
            self._renderFromMapData()
        if self._tileBrushRenderPending:
            self._tileBrushRenderTimer.start(0)

    def _finishTileBrushRender(self) -> None:
        self._tileBrushRenderTimer.stop()
        self._tileBrushRenderPending = False
        self._brushBaseImage = None
        self._brushLayerImage = None
        self._renderFromMapData()

    def _canIncrementalBrushPatch(self) -> bool:
        return (
            self.selectedAutoTileKey is None
            and self.selectedTilePattern is None
            and self._cachedTilesetImage is not None
            and not self._cachedTilesetImage.isNull()
        )

    def _initBrushLayerCache(self) -> bool:
        if self.mapData is None or self.selectedLayerName is None:
            return False
        layerName = self.selectedLayerName
        layer = self.mapData.layers.get(layerName)
        if layer is None or not layer.visible:
            return False
        if self._brushBaseImage is None:
            self._brushBaseImage = self._buildMapImage(skipLayerName=layerName)
        if self._brushBaseImage is None:
            return False
        if self._brushLayerImage is not None:
            return True
        sourceTileSize = EditorStatus.CELLSIZE
        tileSize = self._getTileSize()
        layerImg = self._tilemapRenderer.renderLayer(
            self.mapData.width,
            self.mapData.height,
            tileSize,
            layer.tiles,
            layer.layerTilesetKey,
            layer.autoTiles,
            self._autoTileFrame,
            sourceTileSize,
        )
        if layerImg is None or layerImg.isNull():
            self._brushLayerImage = QtGui.QImage(
                self._brushBaseImage.width(),
                self._brushBaseImage.height(),
                QtGui.QImage.Format_ARGB32,
            )
            self._brushLayerImage.fill(QtCore.Qt.transparent)
        else:
            self._brushLayerImage = layerImg.copy()
        return True

    def _patchBrushLayerTile(self, gx: int, gy: int, tileNum: Optional[int]) -> None:
        if self._brushLayerImage is None:
            return
        tileSize = self._getTileSize()
        sourceTileSize = EditorStatus.CELLSIZE
        dst = QtCore.QRect(gx * tileSize, gy * tileSize, tileSize, tileSize)
        painter = QtGui.QPainter(self._brushLayerImage)
        painter.setCompositionMode(QtGui.QPainter.CompositionMode_Source)
        if tileNum is None:
            painter.fillRect(dst, QtCore.Qt.transparent)
        else:
            columns = self._cachedTilesetImage.width() // sourceTileSize
            rows = self._cachedTilesetImage.height() // sourceTileSize
            n = int(tileNum)
            if columns <= 0 or rows <= 0 or n < 0 or n >= columns * rows:
                painter.fillRect(dst, QtCore.Qt.transparent)
            else:
                tu = n % columns
                tv = n // columns
                src = QtCore.QRect(tu * sourceTileSize, tv * sourceTileSize, sourceTileSize, sourceTileSize)
                painter.drawImage(dst, self._cachedTilesetImage, src)
        painter.end()

    def _compositeBrushPixmap(self) -> None:
        if self._brushBaseImage is None or self._brushLayerImage is None:
            self._renderFromMapDataDuringBrush()
            return
        img = QtGui.QImage(
            self._brushBaseImage.width(),
            self._brushBaseImage.height(),
            QtGui.QImage.Format_ARGB32,
        )
        img.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(img)
        painter.drawImage(0, 0, self._brushBaseImage)
        painter.drawImage(0, 0, self._brushLayerImage)
        layerName = self.selectedLayerName
        if layerName is not None:
            tileSize = self._getTileSize()
            self._drawActorsForLayer(painter, layerName, tileSize, 1.0)
        painter.end()
        self._pixmap = QtGui.QPixmap.fromImage(img)
        self.update()

    def _applyIncrementalBrushCell(self, layer: Any, gx: int, gy: int) -> None:
        if not self._initBrushLayerCache():
            self._scheduleTileBrushRender()
            return
        tiles = layer.tiles
        tileNum: Optional[int] = None
        if isinstance(tiles, list) and 0 <= gy < len(tiles):
            row = tiles[gy]
            if isinstance(row, list) and 0 <= gx < len(row):
                cell = row[gx]
                if isinstance(cell, int):
                    tileNum = cell
        self._patchBrushLayerTile(gx, gy, tileNum)
        self._compositeBrushPixmap()

    def _applyIncrementalBrushPattern(self, layer: Any, gx: int, gy: int) -> None:
        pattern = self.selectedTilePattern
        if not pattern or not self._initBrushLayerCache():
            self._scheduleTileBrushRender()
            return
        if self.mapData is None:
            return
        for py, row in enumerate(pattern):
            if not isinstance(row, list):
                continue
            targetY = gy + py
            if targetY < 0 or targetY >= self.mapData.height:
                continue
            for px, tileNumber in enumerate(row):
                targetX = gx + px
                if targetX < 0 or targetX >= self.mapData.width:
                    continue
                try:
                    self._patchBrushLayerTile(targetX, targetY, int(tileNumber))
                except (TypeError, ValueError):
                    continue
        self._compositeBrushPixmap()

    def _afterBrushCellWrite(self, layer: Any, gx: int, gy: int) -> None:
        self._refreshTitleAfterTileBrush()
        if self._canIncrementalBrushPatch():
            self._applyIncrementalBrushCell(layer, gx, gy)
        else:
            self._scheduleTileBrushRender()

    def _afterBrushPatternWrite(self, layer: Any, gx: int, gy: int) -> None:
        self._refreshTitleAfterTileBrush()
        if self._canIncrementalBrushPatch():
            self._applyIncrementalBrushPattern(layer, gx, gy)
        else:
            self._scheduleTileBrushRender()

    def _onAnimationTick(self) -> None:
        if self.mapData is None:
            return
        if self._tileBrushDragging:
            return
        oldAutoTileFrame = self._autoTileFrame
        elapsed = max(0, self._animationClock.elapsed())
        self._autoTileFrame = int(elapsed / _AUTOTILE_ANIMATION_INTERVAL_MS) % 1024
        self._actorAnimationTime = float(elapsed) / 1000.0
        hasAnimatedAutoTile = self._hasAnyAnimatedAutoTile()
        hasAnimatedActor = self._hasAnyAnimatedActor()
        if not hasAnimatedActor and (not hasAnimatedAutoTile or self._autoTileFrame == oldAutoTileFrame):
            return
        self._renderFromMapData()

    def _hasAnyAnimatedAutoTile(self) -> bool:
        if self.mapData is None:
            return False
        seenAnimated = False
        seenKeys: set = set()
        for layer in self.mapData.layers.values():
            autoTiles = layer.autoTiles
            if not isinstance(autoTiles, list):
                continue
            for row in autoTiles:
                if not isinstance(row, list):
                    continue
                for key in row:
                    if not isinstance(key, str) or not key or key in seenKeys:
                        continue
                    seenKeys.add(key)
                    if self._autoTileRenderer.frameCountFor(key) > 1:
                        seenAnimated = True
                        return seenAnimated
        return seenAnimated

    def _hasAnyAnimatedActor(self) -> bool:
        if self.mapData is None:
            return False
        for layerName in self.mapData.layers.keys():
            actors = self._getActorListForLayer(layerName)
            for entry in actors:
                if not isinstance(entry, dict):
                    continue
                bpRel = entry.get("bp")
                if self._actorPreviewAnimatable(bpRel, entry):
                    return True
        return False

    def invalidateAutoTileCache(self, key: Optional[str] = None) -> None:
        if key is None:
            self._autoTileRenderer.invalidate()
        else:
            self._autoTileRenderer.invalidateKey(key)
        if self.mapData is not None:
            self._renderFromMapData()

    def _updateContentSize(self) -> None:
        if self.mapData is None:
            self.setMinimumSize(0, 0)
            self.setMaximumSize(16777215, 16777215)
            self.updateGeometry()
            return
        size = self._mapPixelSize()
        self.setMinimumSize(size)
        self.setMaximumSize(16777215, 16777215)
        self.updateGeometry()
        parent = self.parentWidget()
        if parent is not None:
            parent.updateGeometry()
            parent.update()

    def _getTileSize(self) -> int:
        try:
            return int(self._tileSize)
        except Exception:
            return int(EditorStatus.CELLSIZE)

    def _setTileSize(self, size: int) -> bool:
        size = max(_MAP_TILE_SIZE_MIN, min(_MAP_TILE_SIZE_MAX, int(size)))
        if size == self._getTileSize():
            return False
        self._tileSize = size
        if self.mapData is not None:
            self._renderFromMapData()
        self._updateContentSize()
        self.update()
        return True

    def _showTileScaleToast(self) -> None:
        w = self.window()
        toast = getattr(w, "toast", None) if isinstance(w, QtWidgets.QWidget) else None
        if not isinstance(toast, Toast):
            return
        scale = int(round(self._baseDisplayScale() * 100.0))
        toast.showMessage(f"{ELOC('scale')}: {scale}%", 1000)

    def _mapPixelSize(self) -> QtCore.QSize:
        if self.mapData is None:
            return QtCore.QSize(0, 0)
        tileSize = self._getTileSize()
        return QtCore.QSize(int(self.mapData.width * tileSize), int(self.mapData.height * tileSize))

    def _mapOffset(self) -> QtCore.QPoint:
        size = self._mapPixelSize()
        x = max(0, int((self.width() - size.width()) / 2))
        y = max(0, int((self.height() - size.height()) / 2))
        return QtCore.QPoint(x, y)

    def _mapDisplayPos(self, pos: QtCore.QPoint) -> QtCore.QPoint:
        return pos - self._mapOffset()

    def _baseDisplayScale(self) -> float:
        baseTileSize = max(1, int(EditorStatus.CELLSIZE))
        return float(self._getTileSize()) / float(baseTileSize)

    def _mapBasePos(self, mapDisplayPos: QtCore.QPoint) -> QtCore.QPointF:
        scale = self._baseDisplayScale()
        if scale <= 0:
            scale = 1.0
        return QtCore.QPointF(float(mapDisplayPos.x()) / scale, float(mapDisplayPos.y()) / scale)

    def _gridPosFromMapDisplayPos(self, mapDisplayPos: QtCore.QPoint) -> Optional[Tuple[int, int]]:
        if self.mapData is None:
            return None
        tileSize = self._getTileSize()
        gx = int(mapDisplayPos.x()) // tileSize
        gy = int(mapDisplayPos.y()) // tileSize
        if gx < 0 or gy < 0 or gx >= self.mapData.width or gy >= self.mapData.height:
            return None
        return gx, gy

    def _parentScrollArea(self) -> Optional[QtWidgets.QScrollArea]:
        parent = self.parentWidget()
        while parent is not None:
            if isinstance(parent, QtWidgets.QScrollArea):
                return parent
            parent = parent.parentWidget()
        return None

    def _ensureHudLabel(self) -> Optional[QtWidgets.QLabel]:
        scroll = self._parentScrollArea()
        if scroll is None:
            return None
        label = self._hudLabel
        if label is None or label.parent() is not scroll:
            label = QtWidgets.QLabel(scroll)
            label.setAttribute(QtCore.Qt.WA_TransparentForMouseEvents, True)
            label.setStyleSheet("color: rgba(255, 255, 255, 0.7); background: transparent;")
            self._hudLabel = label
        label.move(8, 8)
        label.raise_()
        return label

    def _updateGridHud(self) -> None:
        if self._hoverGridPos is None:
            if self._hudLabel is not None:
                self._hudLabel.hide()
            return
        label = self._ensureHudLabel()
        if label is None:
            return
        gx, gy = self._hoverGridPos
        label.setText(f"({gx}, {gy})")
        label.adjustSize()
        label.show()
        label.raise_()

    def mapBasePosFromWidgetPos(self, pos: QtCore.QPoint) -> QtCore.QPointF:
        return self._mapBasePos(self._mapDisplayPos(pos))

    def getLayerNames(self) -> List[str]:
        if self.mapData is None:
            return []
        return list(self.mapData.layers.keys())

    def setSelectedLayer(self, name: Optional[str]) -> None:
        self.selectedLayerName = name
        if name != self.selectedActorLayerName:
            self._setSelectedActor(None, None, render=False)
        self._updateCachedTileset()
        self._renderFromMapData()

    def setTileMode(self, enabled: bool) -> None:
        self.tileModeEnabled = bool(enabled)
        if self.tileModeEnabled:
            self._stopLightRadiusDrag()
            self._stopLightMoveDrag()
            self._setSelectedLightIndex(None)
            self._setLightOverlayEnabled(False)
            self._setSelectedActor(None, None)
        self.update()

    def setLightOverlayEnabled(self, enabled: bool) -> None:
        self._setLightOverlayEnabled(bool(enabled))
        self.update()

    def setPendingActor(self, bpRel: Optional[str]) -> None:
        pending = bpRel.strip() if isinstance(bpRel, str) and bpRel.strip() else None
        if self._pendingActorBpRel == pending:
            return
        self._pendingActorBpRel = pending
        if self.selectedPos is not None:
            self.repaint()
        else:
            self.update()

    def _setLightOverlayEnabled(self, enabled: bool) -> None:
        enabled = bool(enabled)
        if self._lightOverlayEnabled == enabled:
            return
        self._lightOverlayEnabled = enabled
        if enabled:
            self._setSelectedActor(None, None)
        if not enabled:
            self._stopLightRadiusDrag()
            self._stopLightMoveDrag()
            self._setSelectedLightIndex(None)
            self._setSelectedActor(None, None)

    def setSelectedTileNumber(self, num: Optional[int]) -> None:
        self.selectedTileNumber = None if num is None else int(num)
        if self.selectedTileNumber is not None:
            self.selectedTilePattern = None
            self.selectedAutoTileKey = None
        self.update()

    def setSelectedTilePattern(self, pattern: Optional[List[List[int]]]) -> None:
        if not pattern:
            self.selectedTilePattern = None
            self.update()
            return
        normalised: List[List[int]] = []
        for row in pattern:
            if not isinstance(row, list):
                continue
            normalisedRow: List[int] = []
            for value in row:
                try:
                    normalisedRow.append(int(value))
                except Exception:
                    continue
            if normalisedRow:
                normalised.append(normalisedRow)
        self.selectedTilePattern = normalised if normalised else None
        if self.selectedTilePattern is not None:
            self.selectedTileNumber = None
            self.selectedAutoTileKey = None
        self.update()

    def setSelectedAutoTileKey(self, key: Optional[str]) -> None:
        if key is None or not isinstance(key, str) or key == "":
            self.selectedAutoTileKey = None
            self.update()
            return
        self.selectedAutoTileKey = key
        self.selectedTileNumber = None
        self.selectedTilePattern = None
        self.update()

    def clearLightSelection(self) -> None:
        self._stopLightRadiusDrag()
        self._stopLightMoveDrag()
        self._setSelectedLightIndex(None)
        self.update()

    def setSelectedLightIndex(self, index: Optional[int]) -> None:
        self._stopLightRadiusDrag()
        self._stopLightMoveDrag()
        self._setSelectedLightIndex(index)
        self.update()

    def _getSelectedActor(self) -> Optional[Dict[str, Any]]:
        layerName = self.selectedActorLayerName
        index = self.selectedActorIndex
        if not isinstance(layerName, str) or not isinstance(index, int):
            return None
        actors = self._getActorListForLayer(layerName)
        if not (0 <= index < len(actors)):
            return None
        actor = actors[index]
        return actor if isinstance(actor, dict) else None

    def _setSelectedActor(
        self,
        layerName: Optional[str],
        index: Optional[int],
        render: bool = True,
    ) -> None:
        actor = None
        if isinstance(layerName, str) and isinstance(index, int):
            actors = self._getActorListForLayer(layerName)
            if 0 <= index < len(actors) and isinstance(actors[index], dict):
                actor = actors[index]
            else:
                layerName = None
                index = None
        else:
            layerName = None
            index = None
        if self.selectedActorLayerName == layerName and self.selectedActorIndex == index:
            return
        self.selectedActorLayerName = layerName
        self.selectedActorIndex = index
        self.ACTOR_SELECTION_CHANGED.emit(layerName, index, actor)
        if render and self.mapData is not None:
            self._renderFromMapData()
        self.update()

    def copySelectedActor(self) -> None:
        actor = self._getSelectedActor()
        if (
            actor is None
            or not isinstance(self.selectedActorLayerName, str)
            or not isinstance(self.selectedActorIndex, int)
        ):
            return
        self._copyActor(self.selectedActorLayerName, self.selectedActorIndex)

    def pasteActorAtSelectedPos(self) -> None:
        if self.tileModeEnabled or self._lightOverlayEnabled:
            return
        if self.selectedLayerName is None or self.selectedPos is None:
            return
        gx, gy = self.selectedPos
        if self._hasActorAt(self.selectedLayerName, gx, gy):
            return
        self._pasteActor(gx, gy)

    def deleteSelectedActor(self) -> None:
        if self.tileModeEnabled or self._lightOverlayEnabled:
            return
        if not isinstance(self.selectedActorLayerName, str) or not isinstance(self.selectedActorIndex, int):
            return
        self._deleteActor(self.selectedActorLayerName, self.selectedActorIndex)

    def _getLights(self) -> List[Dict[str, Any]]:
        if not self.mapKey:
            return []
        m = GameData.mapData.get(self.mapKey)
        if not isinstance(m, dict):
            return []
        lights = m.get("lights")
        if not isinstance(lights, list):
            return []
        return lights

    def _stopLightRadiusDrag(self) -> None:
        if not self._lightRadiusDragging:
            return
        self._lightRadiusDragging = False
        self._lightRadiusDragMapKey = ""
        self._lightRadiusDragIndex = None
        self._lightRadiusDragCenter = None
        self._lightRadiusDragLastRadius = None
        self._lightRadiusDragTitleRefreshed = False
        self.unsetCursor()

    def _stopLightMoveDrag(self) -> None:
        if not self._lightMoveDragging:
            return
        self._lightMoveDragging = False
        self._lightMoveDragMapKey = ""
        self._lightMoveDragIndex = None
        self._lightMoveDragOffset = None
        self._lightMoveDragTitleRefreshed = False
        self.unsetCursor()

    def _getLightCenterRadius(self, light: Dict[str, Any]) -> Optional[Tuple[float, float, float]]:
        pos = light.get("position")
        if not isinstance(pos, (list, tuple)) or len(pos) < 2:
            return None
        try:
            cx = float(pos[0])
            cy = float(pos[1])
            r = float(light.get("radius", 0.0))
        except Exception:
            return None
        if r <= 0:
            return None
        return cx, cy, r

    def _isInLightDisk(self, pos: QtCore.QPoint, index: int) -> bool:
        lights = self._getLights()
        if not (0 <= index < len(lights)):
            return False
        light = lights[index]
        if not isinstance(light, dict):
            return False
        cr = self._getLightCenterRadius(light)
        if cr is None:
            return False
        cx, cy, r = cr
        dx = float(pos.x()) - cx
        dy = float(pos.y()) - cy
        return (dx * dx + dy * dy) <= r * r

    def _isNearLightEdge(self, pos: QtCore.QPoint, index: int, tol: float = 6.0) -> bool:
        lights = self._getLights()
        if not (0 <= index < len(lights)):
            return False
        light = lights[index]
        if not isinstance(light, dict):
            return False
        cr = self._getLightCenterRadius(light)
        if cr is None:
            return False
        cx, cy, r = cr
        dx = float(pos.x()) - cx
        dy = float(pos.y()) - cy
        dist = (dx * dx + dy * dy) ** 0.5
        return abs(dist - r) <= float(tol)

    def _applyLightRadius(self, index: int, radius: float) -> None:
        if not self.mapKey:
            return
        m = GameData.mapData.get(self.mapKey)
        if not isinstance(m, dict):
            return
        lights = m.get("lights")
        if not isinstance(lights, list):
            return
        if not (0 <= index < len(lights)):
            return
        light = lights[index]
        if not isinstance(light, dict):
            return
        light["radius"] = float(radius)
        self.LIGHT_DATA_CHANGED.emit(self.mapKey, index, light)

    def _applyLightPosition(self, index: int, x: float, y: float) -> None:
        if not self.mapKey:
            return
        m = GameData.mapData.get(self.mapKey)
        if not isinstance(m, dict):
            return
        lights = m.get("lights")
        if not isinstance(lights, list):
            return
        if not (0 <= index < len(lights)):
            return
        light = lights[index]
        if not isinstance(light, dict):
            return
        light["position"] = [float(x), float(y)]
        self.LIGHT_DATA_CHANGED.emit(self.mapKey, index, light)

    def _setSelectedLightIndex(self, index: Optional[int]) -> None:
        if index is not None:
            try:
                index = int(index)
            except Exception:
                index = None
        if index is not None:
            lights = self._getLights()
            if not (0 <= index < len(lights)):
                index = None
        if self.selectedLightIndex == index:
            return
        self.selectedLightIndex = index
        lights = self._getLights()
        light = lights[index] if (index is not None and 0 <= index < len(lights)) else None
        if not isinstance(light, dict):
            light = None
        self.LIGHT_SELECTION_CHANGED.emit(self.mapKey, index, light)

    def _hitTestLight(self, pos: QtCore.QPoint) -> Optional[int]:
        lights = self._getLights()
        if not lights:
            return None
        px = float(pos.x())
        py = float(pos.y())
        best = None
        bestDist2 = None
        for i, l in enumerate(lights):
            if not isinstance(l, dict):
                continue
            p = l.get("position")
            if not isinstance(p, (list, tuple)) or len(p) < 2:
                continue
            try:
                cx = float(p[0])
                cy = float(p[1])
                r = float(l.get("radius", 0.0))
            except Exception:
                continue
            if r <= 0:
                continue
            dx = px - cx
            dy = py - cy
            dist2 = dx * dx + dy * dy
            if dist2 <= r * r:
                if best is None or (bestDist2 is not None and dist2 < bestDist2):
                    best = i
                    bestDist2 = dist2
        return best

    def getLayerTilesetKey(self, name: str) -> Optional[str]:
        if self.mapData is None:
            return None
        if name not in self.mapData.layers:
            return None
        layer = self.mapData.layers[name]
        return layer.layerTilesetKey or None

    def setLayerTilesetForSelectedLayer(self, key: str) -> None:
        if self.mapData is None:
            return
        if self.selectedLayerName is None:
            return
        if key not in GameData.tilesetData:
            return
        GameData.RecordSnapshot()
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        layer.layerTilesetKey = key
        if self.mapKey and self.selectedLayerName in GameData.mapData.get(self.mapKey, {}).get("layers", {}):
            GameData.mapData[self.mapKey]["layers"][self.selectedLayerName]["layerTileset"] = key
        self._refreshTitle()
        self._updateCachedTileset()
        self._renderFromMapData()
        self.update()

    def addEmptyLayer(self, name: Optional[str] = None, filePath: str = "") -> Optional[str]:
        if self.mapData is None:
            return None
        GameData.RecordSnapshot()
        width = self.mapData.width
        height = self.mapData.height
        if not name:
            base = "Layer"
            i = len(self.mapData.layers) + 1
            candidate = f"{base}_{i}"
            while candidate in self.mapData.layers:
                i += 1
                candidate = f"{base}_{i}"
            name = candidate
        tiles: List[List[Optional[int]]] = []
        for y in range(height):
            row: List[Optional[int]] = []
            for x in range(width):
                row.append(None)
            tiles.append(row)
        autoTiles: List[List[Optional[str]]] = []
        for y in range(height):
            autoTiles.append([None] * width)
        keys = list(GameData.tilesetData.keys())
        if not keys:
            return None
        ts_key = keys[0]
        layer = EditorLayerData(name, ts_key, tiles, autoTiles)
        self.mapData.layers[name] = layer
        if self.mapKey:
            if self.mapKey in GameData.mapData:
                if "layers" in GameData.mapData[self.mapKey]:
                    GameData.mapData[self.mapKey]["layers"][name] = {
                        "layerName": name,
                        "layerTileset": ts_key,
                        "tiles": tiles,
                        "autoTiles": [list(r) for r in autoTiles],
                        "shaderPath": "",
                        "actors": [],
                    }
        self._refreshTitle()
        self._renderFromMapData()
        self.update()
        self.DATA_CHANGED.emit()
        return name

    def getLayerShaderPath(self, name: str) -> str:
        if self.mapData is None or name not in self.mapData.layers:
            return ""
        return str(self.mapData.layers[name].shaderPath or "")

    def setLayerShaderPath(self, name: str, shaderPath: str) -> bool:
        if self.mapData is None or name not in self.mapData.layers:
            return False
        normalizedPath = str(shaderPath or "").replace("\\", "/").strip("/")
        if self.getLayerShaderPath(name) == normalizedPath:
            return False
        GameData.RecordSnapshot()
        self.mapData.layers[name].shaderPath = normalizedPath
        if self.mapKey and self.mapKey in GameData.mapData:
            layerData = GameData.mapData[self.mapKey].get("layers", {}).get(name)
            if isinstance(layerData, dict):
                layerData["shaderPath"] = normalizedPath
        self._refreshTitle()
        self.DATA_CHANGED.emit()
        return True

    def removeLayer(self, name: str) -> bool:
        if self.mapData is None:
            return False
        if name not in self.mapData.layers:
            return False
        GameData.RecordSnapshot()
        self.mapData.layers.pop(name, None)
        if self.mapKey and self.mapKey in GameData.mapData:
            GameData.mapData[self.mapKey].get("layers", {}).pop(name, None)
        self._refreshTitle()
        if self.selectedLayerName == name:
            self.selectedLayerName = None
            self._updateCachedTileset()
        self._renderFromMapData()
        self.update()
        self.DATA_CHANGED.emit()
        return True

    def reorderLayers(self, new_order: List[str]) -> None:
        if self.mapData is None:
            return
        current_order = list(self.mapData.layers.keys())
        if new_order == current_order:
            return
        if len(new_order) != len(current_order) or set(new_order) != set(current_order):
            return
        GameData.RecordSnapshot()
        new_layers = {name: self.mapData.layers[name] for name in new_order}
        self.mapData.layers = new_layers
        if self.mapKey and self.mapKey in GameData.mapData:
            game_layers = GameData.mapData[self.mapKey].get("layers", {})
            new_game_layers = {name: game_layers[name] for name in new_order}
            GameData.mapData[self.mapKey]["layers"] = new_game_layers
        self._refreshTitle()
        self._renderFromMapData()
        self.update()
        self.DATA_CHANGED.emit()

    def _commitRectangle(self, endPos: Optional[Tuple[int, int]]) -> None:
        if self.mapData is None or not getattr(self.mapData, "layers", None):
            self.rectStartPos = None
            self.update()
            return
        if self.rectStartPos is None or endPos is None:
            self.rectStartPos = None
            self.update()
            return

        sx, sy = self.rectStartPos
        ex, ey = endPos
        min_x, max_x = min(sx, ex), max(sx, ex)
        min_y, max_y = min(sy, ey), max(sy, ey)

        if not self.tileModeEnabled or self.selectedLayerName is None:
            self.rectStartPos = None
            self.update()
            return

        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            self.rectStartPos = None
            self.update()
            return

        GameData.RecordSnapshot()
        changed = False
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                if self._writeCellSelection(layer, x, y):
                    changed = True

        if changed:
            self._refreshTitle()
            self._renderFromMapData()

        self.rectStartPos = None
        self.update()

    def _writeCellSelection(self, layer, x: int, y: int) -> bool:
        autoKey = self.selectedAutoTileKey
        tileNum = self.selectedTileNumber
        return self._writeCellValue(layer, x, y, tileNum, autoKey)

    def _copyGrid(self, grid: object) -> Grid:
        if not isinstance(grid, list):
            return []
        result: Grid = []
        for row in grid:
            if not isinstance(row, list):
                result.append([])
                continue
            result.append(
                [cell if isinstance(cell, (int, str)) or cell is None else None for cell in row]
            )
        return result

    def _fitGridToTiles(self, grid: Grid, tiles: Grid) -> Grid:
        result: Grid = []
        for y, tileRow in enumerate(tiles):
            srcRow = grid[y] if y < len(grid) and isinstance(grid[y], list) else []
            row: List[GridCell] = []
            for x in range(len(tileRow)):
                row.append(srcRow[x] if x < len(srcRow) else None)
            result.append(row)
        return result

    def _ensureAutoTileCell(self, layer: Any, y: int, x: int) -> List[Optional[str]]:
        tiles = layer.tiles
        autoTiles = layer.autoTiles
        if not isinstance(autoTiles, list):
            autoTiles = []
            layer.autoTiles = autoTiles
        mapH = len(tiles) if isinstance(tiles, list) else 0
        while len(autoTiles) < mapH:
            srcRow = tiles[len(autoTiles)] if isinstance(tiles[len(autoTiles)], list) else []
            autoTiles.append([None] * len(srcRow))
        row = autoTiles[y]
        if not isinstance(row, list):
            srcRow = tiles[y] if isinstance(tiles[y], list) else []
            row = [None] * len(srcRow)
            autoTiles[y] = row
        rowLen = len(tiles[y]) if isinstance(tiles[y], list) else len(row)
        while len(row) < rowLen:
            row.append(None)
        return row

    def _writeCellValue(self, layer, x: int, y: int, tileNum: Optional[int], autoKey: Optional[str]) -> bool:
        tiles = layer.tiles
        if not isinstance(tiles, list) or y < 0 or y >= len(tiles):
            return False
        row = tiles[y]
        if not isinstance(row, list) or x < 0 or x >= len(row):
            return False
        autoRow = self._ensureAutoTileCell(layer, y, x)
        changed = False
        if autoKey is not None:
            if row[x] is not None:
                row[x] = None
                changed = True
            if autoRow[x] != autoKey:
                autoRow[x] = autoKey
                changed = True
        elif tileNum is not None:
            if row[x] != tileNum:
                row[x] = tileNum
                changed = True
            if autoRow[x] is not None:
                autoRow[x] = None
                changed = True
        else:
            if row[x] is not None:
                row[x] = None
                changed = True
            if autoRow[x] is not None:
                autoRow[x] = None
                changed = True
        if changed and self.mapKey and self.selectedLayerName:
            mapEntry = GameData.mapData.get(self.mapKey)
            if isinstance(mapEntry, dict):
                layers = mapEntry.get("layers")
                if isinstance(layers, dict) and self.selectedLayerName in layers:
                    layerData = layers[self.selectedLayerName]
                    layerTiles = layerData.get("tiles")
                    if isinstance(layerTiles, list):
                        layerTiles[y][x] = None if row[x] is None else int(row[x])
                    layerAuto = layerData.get("autoTiles")
                    if not isinstance(layerAuto, list):
                        layerAuto = [list(r) for r in autoTiles]
                        layerData["autoTiles"] = layerAuto
                    if isinstance(layerAuto, list):
                        layerAuto[y][x] = autoRow[x]
        return changed

    def _writeTilePattern(self, layer, x: int, y: int) -> bool:
        pattern = self.selectedTilePattern
        if not pattern:
            return False
        if self.mapData is None:
            return False
        changed = False
        for py, row in enumerate(pattern):
            if not isinstance(row, list):
                continue
            targetY = y + py
            if targetY < 0 or targetY >= self.mapData.height:
                continue
            for px, tileNum in enumerate(row):
                targetX = x + px
                if targetX < 0 or targetX >= self.mapData.width:
                    continue
                if self._writeCellValue(layer, targetX, targetY, int(tileNum), None):
                    changed = True
        return changed

    def mouseReleaseEvent(self, e: QtGui.QMouseEvent) -> None:
        if self._lightOverlayEnabled and e.button() == QtCore.Qt.LeftButton:
            self._stopLightRadiusDrag()
            self._stopLightMoveDrag()
            self._stopActorMoveDrag()
            self.update()
            super().mouseReleaseEvent(e)
            return
        if e.button() == QtCore.Qt.LeftButton and self.rectStartPos is not None:
            self._commitRectangle(self.selectedPos)
        if e.button() == QtCore.Qt.LeftButton and self._tileBrushDragging:
            self._tileBrushDragging = False
            self._finishTileBrushRender()
            self._refreshTitle()
        if e.button() == QtCore.Qt.LeftButton and self._actorMoveDragging:
            self._stopActorMoveDrag()
        super().mouseReleaseEvent(e)

    def keyReleaseEvent(self, e: QtGui.QKeyEvent) -> None:
        if e.key() == QtCore.Qt.Key_Shift and self.rectStartPos is not None:
            self._commitRectangle(self.selectedPos)
        super().keyReleaseEvent(e)

    def paintEvent(self, e: QtGui.QPaintEvent) -> None:
        p = QtGui.QPainter(self)
        opt = QtWidgets.QStyleOption()
        opt.initFrom(self)
        cast(QtWidgets.QStyle, self.style()).drawPrimitive(QtWidgets.QStyle.PE_Widget, opt, p, self)
        mapOffset = self._mapOffset()
        mapSize = self._mapPixelSize()
        r = QtCore.QRect(mapOffset, mapSize)
        s = 16
        c1 = QtGui.QColor(220, 220, 220)
        c2 = QtGui.QColor(180, 180, 180)
        p.save()
        p.setClipRect(r)
        y = r.top()
        while y < r.bottom() + 1:
            x = r.left()
            while x < r.right() + 1:
                c = c1 if (((x // s) + (y // s)) % 2 == 0) else c2
                p.fillRect(QtCore.QRect(x, y, s, s), c)
                x += s
            y += s
        p.restore()
        if self._pixmap is not None:
            p.drawPixmap(mapOffset, self._pixmap)

        p.save()
        p.translate(mapOffset)
        if self._lightOverlayEnabled:
            p.setRenderHint(QtGui.QPainter.Antialiasing, True)
            displayScale = self._baseDisplayScale()
            lights = self._getLights()
            for i, light in enumerate(lights):
                if not isinstance(light, dict):
                    continue
                pos = light.get("position")
                if not isinstance(pos, (list, tuple)) or len(pos) < 2:
                    continue
                try:
                    cx = float(pos[0]) * displayScale
                    cy = float(pos[1]) * displayScale
                    radius = float(light.get("radius", 0.0)) * displayScale
                except Exception:
                    continue
                if radius <= 0:
                    continue
                isSelected = self.selectedLightIndex == i
                color = QtGui.QColor(255, 220, 0) if isSelected else QtGui.QColor(0, 200, 0)
                fillColor = QtGui.QColor(255, 255, 255, 32)
                rawColor = light.get("color")
                if isinstance(rawColor, (list, tuple)) and len(rawColor) >= 3:
                    try:
                        r = max(0, min(255, int(rawColor[0])))
                        g = max(0, min(255, int(rawColor[1])))
                        b = max(0, min(255, int(rawColor[2])))
                        a = max(0, min(255, int(rawColor[3]))) if len(rawColor) >= 4 else 255
                        fillA = max(12, min(80, int(a * 0.15)))
                        fillColor = QtGui.QColor(r, g, b, fillA)
                    except Exception:
                        fillColor = QtGui.QColor(255, 255, 255, 32)

                p.setPen(QtGui.QPen(color, 2))
                p.setBrush(fillColor)
                p.drawEllipse(QtCore.QPointF(cx, cy), radius, radius)
                p.setBrush(color)
                p.setPen(QtCore.Qt.NoPen)
                p.drawEllipse(QtCore.QPointF(cx, cy), 3.0, 3.0)
                p.setBrush(QtCore.Qt.NoBrush)

        if self.selectedPos is not None and self.selectedLayerName is not None:
            gx, gy = self.selectedPos
            sourceTileSize = EditorStatus.CELLSIZE
            tileSize = self._getTileSize()
            displayScale = self._baseDisplayScale()

            if self.rectStartPos is not None:
                sx, sy = self.rectStartPos
                min_x, max_x = min(sx, gx), max(sx, gx)
                min_y, max_y = min(sy, gy), max(sy, gy)
            else:
                min_x, max_x = gx, gx
                min_y, max_y = gy, gy

            tilePattern = self.selectedTilePattern if self.tileModeEnabled else None
            if tilePattern is not None and self.rectStartPos is None:
                patternHeight = len(tilePattern)
                patternWidth = max((len(row) for row in tilePattern if isinstance(row, list)), default=0)
                if patternWidth > 0 and patternHeight > 0:
                    min_x, max_x = gx, gx + patternWidth - 1
                    min_y, max_y = gy, gy + patternHeight - 1

            if (
                self.tileModeEnabled
                and tilePattern is None
                and self.selectedTileNumber is not None
                and self._cachedTilesetImage is not None
                and not self._cachedTilesetImage.isNull()
            ):
                n = int(self.selectedTileNumber)
                columns = self._cachedTilesetImage.width() // sourceTileSize
                rows = self._cachedTilesetImage.height() // sourceTileSize
                total = columns * rows
                if 0 <= n < total:
                    tu = n % columns
                    tv = n // columns
                    src = QtCore.QRect(tu * sourceTileSize, tv * sourceTileSize, sourceTileSize, sourceTileSize)

                    p.setOpacity(0.5)
                    for y_idx in range(min_y, max_y + 1):
                        for x_idx in range(min_x, max_x + 1):
                            dst = QtCore.QRect(x_idx * tileSize, y_idx * tileSize, tileSize, tileSize)
                            p.drawImage(dst, self._cachedTilesetImage, src)
                    p.setOpacity(1.0)

            if (
                self.tileModeEnabled
                and tilePattern is not None
                and self._cachedTilesetImage is not None
                and not self._cachedTilesetImage.isNull()
            ):
                columns = self._cachedTilesetImage.width() // sourceTileSize
                rows = self._cachedTilesetImage.height() // sourceTileSize
                total = columns * rows
                if total > 0:
                    p.setOpacity(0.5)
                    for py, row in enumerate(tilePattern):
                        if not isinstance(row, list):
                            continue
                        for px, tileNumber in enumerate(row):
                            try:
                                n = int(tileNumber)
                            except Exception:
                                continue
                            if n < 0 or n >= total:
                                continue
                            tu = n % max(1, columns)
                            tv = n // max(1, columns)
                            src = QtCore.QRect(
                                tu * sourceTileSize, tv * sourceTileSize, sourceTileSize, sourceTileSize
                            )
                            dst = QtCore.QRect((gx + px) * tileSize, (gy + py) * tileSize, tileSize, tileSize)
                            p.drawImage(dst, self._cachedTilesetImage, src)
                    p.setOpacity(1.0)

            if self.tileModeEnabled and self.selectedAutoTileKey is not None:
                preview = self._autoTileRenderer.renderTile(self.selectedAutoTileKey, 0, self._autoTileFrame)
                if preview is not None and not preview.isNull():
                    p.setOpacity(0.5)
                    for y_idx in range(min_y, max_y + 1):
                        for x_idx in range(min_x, max_x + 1):
                            dst = QtCore.QRect(x_idx * tileSize, y_idx * tileSize, tileSize, tileSize)
                            p.drawImage(dst, preview)
                    p.setOpacity(1.0)

            p.setPen(QtGui.QPen(QtCore.Qt.black, 1))
            p.setBrush(QtCore.Qt.NoBrush)

            if (
                not self.tileModeEnabled
                and isinstance(self._pendingActorBpRel, str)
                and self._pendingActorBpRel.strip()
                and self.selectedLayerName is not None
            ):
                gx0, gy0 = gx, gy
                if self._hasActorAt(self.selectedLayerName, gx0, gy0):
                    w = sourceTileSize
                    h = sourceTileSize
                    defOrigin = self._toVec2f(
                        self.getBlueprintAttr(self._pendingActorBpRel, "defaultOrigin", (0.0, 0.0)), 0.0, 0.0
                    )
                    origin = self._toVec2f(defOrigin, defOrigin[0], defOrigin[1])
                    px = gx0 * tileSize
                    py = gy0 * tileSize
                    p.save()
                    p.translate(px, py)
                    p.scale(displayScale, displayScale)
                    p.setOpacity(0.5)
                    color = QtGui.QColor(255, 0, 0, 120)
                    rr = QtCore.QRectF(-origin[0], -origin[1], w, h)
                    p.fillRect(rr, color)
                    p.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 180), 1))
                    p.drawRect(rr)
                    p.setOpacity(1.0)
                    p.restore()
                else:
                    bpRel = self._pendingActorBpRel
                    defTrans = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultTranslation", (0.0, 0.0)), 0.0, 0.0)
                    translation = self._toVec2f(defTrans, defTrans[0], defTrans[1])
                    defRot = self.getBlueprintAttr(bpRel, "defaultRotation", 0.0)
                    rotation = float(defRot)
                    defScale = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultScale", (1.0, 1.0)), 1.0, 1.0)
                    scaleVal = self._toVec2f(defScale, defScale[0], defScale[1])
                    defOrigin = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultOrigin", (0.0, 0.0)), 0.0, 0.0)
                    origin = self._toVec2f(defOrigin, defOrigin[0], defOrigin[1])
                    texPath = self.getBlueprintAttr(bpRel, "texturePath", "")
                    shaderPath = self.getBlueprintAttr(bpRel, "shaderPath", "")
                    hue = self._normaliseActorHue(self.getBlueprintAttr(bpRel, "hue", 0.0))
                    rectT = self._toRectTuple(self.getBlueprintAttr(bpRel, "defaultRect", None))
                    px = gx0 * tileSize
                    py = gy0 * tileSize
                    p.save()
                    p.translate(px, py)
                    p.scale(displayScale, displayScale)
                    p.translate(translation[0], translation[1])
                    p.rotate(rotation)
                    p.scale(scaleVal[0], scaleVal[1])
                    w = sourceTileSize
                    h = sourceTileSize
                    sx = 0
                    sy = 0
                    if rectT:
                        sx, sy, w, h = rectT
                    imgGhost = self._resolveTextureImage(texPath)
                    shaderGhost = self._renderActorShaderImage(
                        texPath, shaderPath, rectT, imgGhost.width() if imgGhost is not None else 0
                    )
                    p.setOpacity(0.5)
                    if shaderGhost is not None:
                        drawImg = self._applyActorHueToImage(shaderGhost, hue)
                        src = QtCore.QRectF(0, 0, drawImg.width(), drawImg.height())
                        dst = QtCore.QRectF(-origin[0], -origin[1], w, h)
                        p.drawImage(dst, drawImg, src)
                    elif imgGhost is not None and rectT is not None:
                        dst = QtCore.QRectF(-origin[0], -origin[1], w, h)
                        if self._isNeutralActorHue(hue):
                            src = QtCore.QRectF(sx, sy, w, h)
                            p.drawImage(dst, imgGhost, src)
                        else:
                            drawImg = self._applyActorHueToImage(imgGhost.copy(sx, sy, w, h), hue)
                            src = QtCore.QRectF(0, 0, drawImg.width(), drawImg.height())
                            p.drawImage(dst, drawImg, src)
                    else:
                        color = QtGui.QColor(0, 120, 255, 120)
                        rr = QtCore.QRectF(-origin[0], -origin[1], w, h)
                        p.fillRect(rr, color)
                        p.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 180), 1))
                        p.drawRect(rr)
                    p.setOpacity(1.0)
                    p.restore()

            rect_w = (max_x - min_x + 1) * tileSize
            rect_h = (max_y - min_y + 1) * tileSize
            p.drawRect(min_x * tileSize, min_y * tileSize, rect_w - 1, rect_h - 1)

        p.restore()
        p.end()

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Utils.Panel.ApplyDisabledOpacity(self)
        super().changeEvent(e)

    def leaveEvent(self, a0: QtCore.QEvent) -> None:
        self._stopLightRadiusDrag()
        self._stopLightMoveDrag()
        self.selectedPos = None
        self._hoverGridPos = None
        self._updateGridHud()
        self.update()
        super().leaveEvent(a0)

    def resizeEvent(self, e: QtGui.QResizeEvent) -> None:
        self.update()
        super().resizeEvent(e)

    def wheelEvent(self, e: QtGui.QWheelEvent) -> None:
        if e.modifiers() & QtCore.Qt.ControlModifier:
            delta = e.angleDelta().y()
            if delta != 0:
                stepCount = int(delta / 120)
                if stepCount == 0:
                    stepCount = 1 if delta > 0 else -1
                self._setTileSize(self._getTileSize() + stepCount * _MAP_TILE_SIZE_STEP)
                self._showTileScaleToast()
                e.accept()
                return
        super().wheelEvent(e)

    def _hasActorAt(self, layerName: str, gx: int, gy: int) -> bool:
        actors = self._getActorListForLayer(layerName)
        for entry in actors:
            if not isinstance(entry, dict):
                continue
            pos = entry.get("position")
            if not isinstance(pos, (list, tuple)) or len(pos) < 2:
                continue
            try:
                ax = int(pos[0])
                ay = int(pos[1])
                if ax == gx and ay == gy:
                    return True
            except Exception:
                continue
        return False

    def _copyActor(self, layerName: str, index: int) -> None:
        actors = self._getActorListForLayer(layerName)
        if 0 <= index < len(actors):
            actor = actors[index]
            self._actorClipboard = copy.deepcopy(actor)
            mapData = GameData.mapData.get(self.mapKey)
            if isinstance(actor, dict) and isinstance(mapData, dict):
                self._actorClassVarChangesClipboard = self._getActorClassVarChanges(mapData, str(actor.get("tag", "")))
            else:
                self._actorClassVarChangesClipboard = None

    def _pasteActor(self, gx: int, gy: int) -> None:
        if self._actorClipboard is None or self.selectedLayerName is None:
            return
        if not self.mapKey or self.mapKey not in GameData.mapData:
            return

        GameData.RecordSnapshot()
        newActor = copy.deepcopy(self._actorClipboard)
        newActor["position"] = [gx, gy]

        bpRel = newActor.get("bp", "")
        clsObj = self._resolveActorClass(bpRel)
        newTag = self._makeUniqueDefaultTag(clsObj, bpRel, self.selectedLayerName, gx, gy)
        newActor["tag"] = newTag
        GameData.CleanActorInstanceTransformData(newActor)

        m = GameData.mapData[self.mapKey]
        actorsDict = m.get("actors")
        if not isinstance(actorsDict, dict):
            actorsDict = {}
            m["actors"] = actorsDict

        layerList = actorsDict.get(self.selectedLayerName)
        if not isinstance(layerList, list):
            layerList = []
            actorsDict[self.selectedLayerName] = layerList

        layerList.append(newActor)
        self._pasteActorClassVarChanges(m, newTag)
        self._setSelectedActor(self.selectedLayerName, len(layerList) - 1, render=False)
        self._refreshTitle()
        self.DATA_CHANGED.emit()
        self._renderFromMapData()
        self.update()

    def _placeActorBlueprintAt(self, bpRel: str, gx: int, gy: int) -> bool:
        if self.tileModeEnabled or self._lightOverlayEnabled:
            return False
        if self.selectedLayerName is None or not self.mapKey or self.mapKey not in GameData.mapData:
            return False
        if self._hasActorAt(self.selectedLayerName, gx, gy):
            return False
        bpRel = bpRel.strip() if isinstance(bpRel, str) else ""
        if not bpRel:
            return False
        m = GameData.mapData[self.mapKey]
        actorsDict = m.get("actors")
        if not isinstance(actorsDict, dict):
            actorsDict = {}
            m["actors"] = actorsDict
        layerKey = self.selectedLayerName
        layerList = actorsDict.get(layerKey)
        if not isinstance(layerList, list):
            layerList = []
            actorsDict[layerKey] = layerList
        clsObj = self._resolveActorClass(bpRel)
        tagStr = self._makeUniqueDefaultTag(clsObj, bpRel, layerKey, gx, gy)
        actorEntry = {
            "tag": tagStr,
            "bp": bpRel,
            "position": [gx, gy],
        }
        GameData.RecordSnapshot()
        layerList.append(actorEntry)
        self._setSelectedActor(layerKey, len(layerList) - 1, render=False)
        self._refreshTitle()
        self.DATA_CHANGED.emit()
        self._renderFromMapData()
        self.update()
        return True

    def _deleteActor(self, layerName: str, index: int) -> None:
        if not self.mapKey or self.mapKey not in GameData.mapData:
            return
        m = GameData.mapData[self.mapKey]
        actorsDict = m.get("actors", {})
        layerList = actorsDict.get(layerName)

        if isinstance(layerList, list) and 0 <= index < len(layerList):
            GameData.RecordSnapshot()
            entry = layerList[index]
            if isinstance(entry, dict):
                self._removeActorClassVarChanges(m, str(entry.get("tag", "")))
            layerList.pop(index)
            self._refreshTitle()
            self.DATA_CHANGED.emit()
            self._setSelectedActor(None, None, render=False)
            self._renderFromMapData()
            self.update()

    def _removeActorClassVarChanges(self, mapData: Dict[str, Any], tag: str) -> None:
        root = mapData.get("BPClassVarChanged")
        if not isinstance(root, dict):
            return
        root.pop(tag, None)
        if not root:
            mapData.pop("BPClassVarChanged", None)

    def _getActorClassVarChanges(self, mapData: Dict[str, Any], tag: str) -> Optional[Dict[str, Any]]:
        root = mapData.get("BPClassVarChanged")
        if not isinstance(root, dict):
            return None
        changes = root.get(tag)
        if not isinstance(changes, dict) or not changes:
            return None
        return copy.deepcopy(changes)

    def _pasteActorClassVarChanges(self, mapData: Dict[str, Any], tag: str) -> None:
        changes = self._actorClassVarChangesClipboard
        if not isinstance(changes, dict) or not changes or not tag:
            return
        root = mapData.get("BPClassVarChanged")
        if not isinstance(root, dict):
            root = {}
            mapData["BPClassVarChanged"] = root
        root[tag] = copy.deepcopy(changes)

    def _moveActorClassVarChanges(self, mapData: Dict[str, Any], oldTag: str, newTag: str) -> None:
        if oldTag == newTag:
            return
        root = mapData.get("BPClassVarChanged")
        if not isinstance(root, dict):
            return
        oldChanges = root.pop(oldTag, None)
        if not isinstance(oldChanges, dict):
            if not root:
                mapData.pop("BPClassVarChanged", None)
            return
        newChanges = root.get(newTag)
        if isinstance(newChanges, dict):
            newChanges.update(oldChanges)
        else:
            root[newTag] = oldChanges
        if not root:
            mapData.pop("BPClassVarChanged", None)

    def hitTestLightFromWidgetPos(self, pos: QtCore.QPoint) -> Optional[int]:
        if not self._lightOverlayEnabled:
            return None
        basePos = self._mapBasePos(self._mapDisplayPos(pos))
        return self._hitTestLight(basePos)

    def deleteLight(self, index: int) -> None:
        self._deleteLight(index)

    def _deleteLight(self, index: int) -> None:
        if not self._lightOverlayEnabled:
            return
        if not self.mapKey or self.mapKey not in GameData.mapData:
            return
        m = GameData.mapData[self.mapKey]
        lights = m.get("lights")
        if not isinstance(lights, list):
            return
        if not (0 <= index < len(lights)):
            return
        GameData.RecordSnapshot()
        lights.pop(index)
        self._refreshTitle()
        self.DATA_CHANGED.emit()
        self._setSelectedLightIndex(None)
        self._renderFromMapData()
        self.update()

    def mousePressEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.mapData is None:
            return
        mapPos = self._mapDisplayPos(e.pos())
        basePos = self._mapBasePos(mapPos)
        if self._lightOverlayEnabled:
            if e.button() != QtCore.Qt.LeftButton:
                return

            idx = self.selectedLightIndex if isinstance(self.selectedLightIndex, int) else None
            if isinstance(idx, int) and self._isNearLightEdge(basePos, idx):
                lights = self._getLights()
                if 0 <= idx < len(lights) and isinstance(lights[idx], dict):
                    cr = self._getLightCenterRadius(lights[idx])
                    if cr is not None:
                        cx, cy, r = cr
                        GameData.RecordSnapshot()
                        self._lightRadiusDragging = True
                        self._lightRadiusDragMapKey = self.mapKey
                        self._lightRadiusDragIndex = idx
                        self._lightRadiusDragCenter = (cx, cy)
                        self._lightRadiusDragLastRadius = r
                        self._lightRadiusDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return

            if isinstance(idx, int) and self._isInLightDisk(basePos, idx):
                lights = self._getLights()
                if 0 <= idx < len(lights) and isinstance(lights[idx], dict):
                    cr = self._getLightCenterRadius(lights[idx])
                    if cr is not None:
                        cx, cy, _ = cr
                        GameData.RecordSnapshot()
                        self._lightMoveDragging = True
                        self._lightMoveDragMapKey = self.mapKey
                        self._lightMoveDragIndex = idx
                        self._lightMoveDragOffset = (float(basePos.x()) - cx, float(basePos.y()) - cy)
                        self._lightMoveDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return

            hit = self._hitTestLight(basePos)
            self._setSelectedLightIndex(hit)
            if isinstance(hit, int) and self._isInLightDisk(basePos, hit) and not self._isNearLightEdge(basePos, hit):
                lights = self._getLights()
                if 0 <= hit < len(lights) and isinstance(lights[hit], dict):
                    cr = self._getLightCenterRadius(lights[hit])
                    if cr is not None:
                        cx, cy, _ = cr
                        GameData.RecordSnapshot()
                        self._lightMoveDragging = True
                        self._lightMoveDragMapKey = self.mapKey
                        self._lightMoveDragIndex = hit
                        self._lightMoveDragOffset = (float(basePos.x()) - cx, float(basePos.y()) - cy)
                        self._lightMoveDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return
            self.update()
            return
        tileSize = self._getTileSize()
        gridPos = self._gridPosFromMapDisplayPos(mapPos)
        if gridPos is None:
            return
        gx, gy = gridPos
        self.selectedPos = (gx, gy)
        if not self.tileModeEnabled:
            if self.selectedLayerName is not None:
                hit = self._hitTestActor(self.selectedLayerName, mapPos, tileSize)

                if e.button() == QtCore.Qt.RightButton:
                    if isinstance(hit, int):
                        self._setSelectedActor(self.selectedLayerName, hit)
                    menu = QtWidgets.QMenu(self)

                    actCopy = QtWidgets.QAction(ELOC("COPY"), self)
                    actCopy.setEnabled(isinstance(hit, int))
                    actCopy.triggered.connect(lambda: self._copyActor(self.selectedLayerName, hit))
                    menu.addAction(actCopy)

                    actPaste = QtWidgets.QAction(ELOC("PASTE"), self)
                    hasActor = self._hasActorAt(self.selectedLayerName, gx, gy)
                    canPaste = (self._actorClipboard is not None) and (hit is None) and (not hasActor)
                    actPaste.setEnabled(canPaste)
                    actPaste.triggered.connect(lambda: self._pasteActor(gx, gy))
                    menu.addAction(actPaste)

                    actDelete = QtWidgets.QAction(ELOC("DELETE"), self)
                    actDelete.setEnabled(isinstance(hit, int))
                    actDelete.triggered.connect(lambda: self._deleteActor(self.selectedLayerName, hit))
                    menu.addAction(actDelete)

                    itemData = {"layer": self.selectedLayerName, "grid": (gx, gy)}
                    if isinstance(hit, int):
                        actors = self._getActorListForLayer(self.selectedLayerName)
                        if 0 <= hit < len(actors):
                            itemData["index"] = hit
                            itemData["actor"] = actors[hit]
                    from Utils import PluginSystem

                    PluginSystem.AddRightClickActions(
                        menu,
                        self,
                        "editorPanel_actor",
                        "hit" if isinstance(hit, int) else "empty",
                        itemData,
                    )
                    menu.exec_(e.globalPos())
                    return

                if e.button() == QtCore.Qt.LeftButton:
                    if isinstance(hit, int):
                        self._setSelectedActor(self.selectedLayerName, hit)

                        GameData.RecordSnapshot()
                        self._actorMoveDragging = True
                        self._actorMoveDragLayerName = self.selectedLayerName
                        self._actorMoveDragIndex = hit
                        self._actorMoveDragLastGrid = (gx, gy)
                        self._actorMoveDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return
                    else:
                        if isinstance(self._pendingActorBpRel, str) and self._pendingActorBpRel.strip():
                            if self._placeActorBlueprintAt(self._pendingActorBpRel, gx, gy):
                                return
                        self._setSelectedActor(None, None)
            return
        if self.selectedLayerName is None:
            return
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        if e.button() == QtCore.Qt.RightButton:
            try:
                autoTiles = layer.autoTiles
                pickedAuto: Optional[str] = None
                if isinstance(autoTiles, list) and 0 <= gy < len(autoTiles):
                    rowAuto = autoTiles[gy]
                    if isinstance(rowAuto, list) and 0 <= gx < len(rowAuto):
                        cell = rowAuto[gx]
                        if isinstance(cell, str) and cell:
                            pickedAuto = cell
                if pickedAuto is not None:
                    self.AUTOTILE_PICKED.emit(pickedAuto)
                else:
                    tn = layer.tiles[gy][gx]
                    self.TILE_NUMBER_PICKED.emit(-1 if tn is None else int(tn))
            except Exception:
                self.TILE_NUMBER_PICKED.emit(-1)
            return
        if e.button() != QtCore.Qt.LeftButton:
            return

        if self.selectedTilePattern is not None:
            self._tileBrushDragging = True
            self._brushBaseImage = None
            self._brushLayerImage = None
            GameData.RecordSnapshot()
            if self._writeTilePattern(layer, gx, gy):
                self._afterBrushPatternWrite(layer, gx, gy)
            self.update()
            return

        if e.modifiers() & QtCore.Qt.ShiftModifier:
            self.rectStartPos = (gx, gy)
            return

        self._tileBrushDragging = True
        self._brushBaseImage = None
        self._brushLayerImage = None
        GameData.RecordSnapshot()
        if self._writeCellSelection(layer, gx, gy):
            self._afterBrushCellWrite(layer, gx, gy)
        self.update()

    def mouseMoveEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.mapData is None:
            return
        mapPos = self._mapDisplayPos(e.pos())
        basePos = self._mapBasePos(mapPos)
        gridPos = self._gridPosFromMapDisplayPos(mapPos)
        if gridPos != self._hoverGridPos:
            self._hoverGridPos = gridPos
            self._updateGridHud()
        if self._lightOverlayEnabled:
            if self._lightMoveDragging:
                idx = self._lightMoveDragIndex
                offset = self._lightMoveDragOffset
                if isinstance(idx, int) and isinstance(offset, tuple) and len(offset) == 2:
                    ox, oy = float(offset[0]), float(offset[1])
                    x = float(basePos.x()) - ox
                    y = float(basePos.y()) - oy
                    self._applyLightPosition(idx, x, y)
                    if not self._lightMoveDragTitleRefreshed:
                        self._lightMoveDragTitleRefreshed = True
                        self._refreshTitle()
                    self.update()
                return

            if self._lightRadiusDragging:
                idx = self._lightRadiusDragIndex
                center = self._lightRadiusDragCenter
                if isinstance(idx, int) and isinstance(center, tuple) and len(center) == 2:
                    cx, cy = float(center[0]), float(center[1])
                    dx = float(basePos.x()) - cx
                    dy = float(basePos.y()) - cy
                    radius = max(0.0, (dx * dx + dy * dy) ** 0.5)
                    last = self._lightRadiusDragLastRadius
                    if last is None or abs(radius - float(last)) >= 0.5:
                        self._lightRadiusDragLastRadius = radius
                        self._applyLightRadius(idx, radius)
                        if not self._lightRadiusDragTitleRefreshed:
                            self._lightRadiusDragTitleRefreshed = True
                            self._refreshTitle()
                        self.update()
                return

            if isinstance(self.selectedLightIndex, int) and self._isNearLightEdge(basePos, self.selectedLightIndex):
                self.setCursor(QtCore.Qt.SizeAllCursor)
            else:
                self.unsetCursor()
            return
        if gridPos is None:
            if self.selectedPos is not None:
                self.selectedPos = None
                self.update()
            return
        gx, gy = gridPos

        if self.selectedPos != (gx, gy):
            self.selectedPos = (gx, gy)
            self.update()

        if self.rectStartPos is not None:
            if not (e.modifiers() & QtCore.Qt.ShiftModifier):
                self._commitRectangle((gx, gy))
            else:
                self.update()
            return

        if not self.tileModeEnabled:
            if (
                self._actorMoveDragging
                and isinstance(self._actorMoveDragIndex, int)
                and isinstance(self._actorMoveDragLayerName, str)
            ):
                if (self._actorMoveDragLastGrid is None) or (self._actorMoveDragLastGrid != (gx, gy)):
                    self._actorMoveDragLastGrid = (gx, gy)
                    m = GameData.mapData.get(self.mapKey)
                    if isinstance(m, dict):
                        actorsDict = m.get("actors")
                        if isinstance(actorsDict, dict):
                            layerList = actorsDict.get(self._actorMoveDragLayerName)
                            if isinstance(layerList, list) and 0 <= self._actorMoveDragIndex < len(layerList):
                                entry = layerList[self._actorMoveDragIndex]
                                if isinstance(entry, dict):
                                    oldPos = entry.get("position")
                                    entry["position"] = [gx, gy]
                                    try:
                                        bpRel = entry.get("bp", "")
                                        clsObj = self._resolveActorClass(bpRel)
                                        if isinstance(oldPos, (list, tuple)) and len(oldPos) >= 2:
                                            defOld = self._makeDefaultTag(
                                                clsObj,
                                                bpRel,
                                                self._actorMoveDragLayerName,
                                                int(oldPos[0]),
                                                int(oldPos[1]),
                                            )
                                            if self._getDefaultTagSuffix(entry.get("tag"), defOld) is not None:
                                                oldTag = str(entry.get("tag", ""))
                                                newTag = self._makeUniqueDefaultTag(
                                                    clsObj,
                                                    bpRel,
                                                    self._actorMoveDragLayerName,
                                                    gx,
                                                    gy,
                                                    self._actorMoveDragLayerName,
                                                    self._actorMoveDragIndex,
                                                )
                                                entry["tag"] = newTag
                                                self._moveActorClassVarChanges(m, oldTag, newTag)
                                    except Exception:
                                        pass
                                    if not self._actorMoveDragTitleRefreshed:
                                        self._actorMoveDragTitleRefreshed = True
                                        self._refreshTitle()
                                    self._renderFromMapData()
                                    self.update()
                return
            return
        if self.selectedLayerName is None:
            return
        if not (e.buttons() & QtCore.Qt.LeftButton):
            return
        if self.selectedTilePattern is not None:
            self.update()
            return
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        try:
            if self._writeCellSelection(layer, gx, gy):
                self._afterBrushCellWrite(layer, gx, gy)
        except Exception:
            return
        self.update()

    def saveFile(self) -> Tuple[bool, str]:
        if self.mapData is None:
            return False, ELOC("MAP_DATA_NONE")
        if self.mapFilePath is None:
            return False, ELOC("MAP_FILE_NONE")
        try:
            if self.mapKey and self.mapKey in GameData.mapData:
                mapData = GameData.mapData[self.mapKey]
                if not isinstance(mapData, dict):
                    return False, ELOC("MAP_DATA_NONE")
                GameData.CleanMapActorInstanceTransformData(mapData)
                payload = GameData.GetMapSavePayload(mapData)
                isJson = bool(payload.pop("isJson", None)) or GameData.GetDataFormat("Maps", self.mapKey) == DATA_FORMAT_JSON
                ext = DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON] if isJson else DATA_FORMAT_EXTENSIONS[DATA_FORMAT_DAT]
                savePath = self.mapFilePath
                if os.path.splitext(savePath)[1].lower() not in DATA_FILE_EXTENSIONS:
                    savePath = os.path.join(self._mapFilesRoot, self.mapKey + ext)
                if isJson:
                    Utils.File.SaveJSONData(savePath, payload)
                else:
                    Utils.File.SaveData(savePath, payload)
                self.mapFilePath = savePath
            else:
                return False, ELOC("MAP_DATA_NONE")
        except Exception as e:
            return False, str(e)
        return True, ELOC("SAVE_PATH").format(self.mapFilePath)

    def renameLayer(self, oldName: str, newName: str) -> bool:
        if self.mapData is None:
            return False
        old = oldName.strip() if isinstance(oldName, str) else None
        new = newName.strip() if isinstance(newName, str) else None
        if not old or not new:
            return False
        if old not in self.mapData.layers:
            return False
        if new in self.mapData.layers:
            return False
        GameData.RecordSnapshot()
        newLayers = {}
        for layerName, layer in self.mapData.layers.items():
            if layerName == old:
                layer.layerName = new
                newLayers[new] = layer
            else:
                newLayers[layerName] = layer
        self.mapData.layers = newLayers
        if self.mapKey and self.mapKey in GameData.mapData:
            mapDict = GameData.mapData[self.mapKey]
            layersDict = mapDict.get("layers", {})
            if isinstance(layersDict, dict):
                newLayersDict = {}
                for layerName, data in layersDict.items():
                    if layerName == old:
                        if isinstance(data, dict):
                            data["layerName"] = new
                        newLayersDict[new] = data
                    else:
                        newLayersDict[layerName] = data
                mapDict["layers"] = newLayersDict
            actorsDict = mapDict.get("actors")
            if isinstance(actorsDict, dict) and old in actorsDict:
                oldActors = actorsDict.get(old)
                if isinstance(oldActors, list):
                    for index, entry in enumerate(oldActors):
                        if not isinstance(entry, dict):
                            continue
                        oldBaseTag = self._getActorDefaultTagBase(entry, old)
                        if self._getDefaultTagSuffix(entry.get("tag"), oldBaseTag) is None:
                            continue
                        newBaseTag = self._getActorDefaultTagBase(entry, new)
                        if newBaseTag is None:
                            continue
                        oldTag = str(entry.get("tag", ""))
                        newTag = self.makeUniqueActorTag(newBaseTag, old, index)
                        entry["tag"] = newTag
                        self._moveActorClassVarChanges(mapDict, oldTag, newTag)
                newActorsDict = {}
                for layerName, actors in actorsDict.items():
                    if layerName == old:
                        newActorsDict[new] = actors
                    else:
                        newActorsDict[layerName] = actors
                mapDict["actors"] = newActorsDict
        if self.selectedLayerName == old:
            self.selectedLayerName = new
        self._refreshTitle()
        self._renderFromMapData()
        self.update()
        self.DATA_CHANGED.emit()
        return True

    def _refreshTitle(self) -> None:
        try:
            w = self.window()
            if not w:
                return
            w.setWindowTitle(System.GetTitle())
            self.DATA_CHANGED.emit()
        except Exception as e:
            print(f"Error while refreshing title: {e}")

    def _refreshTitleAfterTileBrush(self) -> None:
        if self._tileBrushDragging:
            return
        self._refreshTitle()

    def _getActorListForLayer(self, layerName: str) -> List[Dict[str, Any]]:
        if not self.mapKey or not isinstance(layerName, str):
            return []
        m = GameData.mapData.get(self.mapKey)
        if not isinstance(m, dict):
            return []
        actorsDict = m.get("actors")
        if not isinstance(actorsDict, dict):
            return []
        layerList = actorsDict.get(layerName)
        if not isinstance(layerList, list):
            return []
        return layerList

    def _toRectTuple(self, rectData: Any) -> Optional[Tuple[int, int, int, int]]:
        if not isinstance(rectData, (list, tuple)) or len(rectData) < 2:
            return None
        a, b = rectData[0], rectData[1]
        if not isinstance(a, (list, tuple)) or not isinstance(b, (list, tuple)):
            return None
        if len(a) < 2 or len(b) < 2:
            return None
        try:
            x = int(a[0])
            y = int(a[1])
            w = int(b[0])
            h = int(b[1])
            if w <= 0 or h <= 0:
                return None
            return (x, y, w, h)
        except Exception:
            return None

    def _toVec2f(self, data: Any, defaultX: float = 0.0, defaultY: float = 0.0) -> Tuple[float, float]:
        if isinstance(data, (list, tuple)) and len(data) >= 2:
            try:
                return (float(data[0]), float(data[1]))
            except Exception:
                return (defaultX, defaultY)
        return (defaultX, defaultY)

    def _toBool(self, data: Any, default: bool = False) -> bool:
        if isinstance(data, bool):
            return data
        if isinstance(data, str):
            text = data.strip().lower()
            if text in ("true", "1", "yes", "on"):
                return True
            if text in ("false", "0", "no", "off"):
                return False
        if data is None:
            return default
        return bool(data)

    def _toPositiveFloat(self, data: Any, default: float) -> float:
        try:
            value = float(data)
        except (TypeError, ValueError):
            value = float(default)
        if value <= 0.0:
            value = float(default)
        return value

    def _resolveActorClass(self, bpRel: Optional[str]) -> Optional[type]:
        if not isinstance(bpRel, str) or not bpRel.strip():
            return None
        try:
            return GameData.classDict.get(bpRel, EditorStatus.PROJ_PATH)
        except Exception:
            return None

    def _getClassAttr(self, cls: Optional[type], name: str, default: Any) -> Any:
        if isinstance(cls, type):
            try:
                return getattr(cls, name)
            except AttributeError:
                return default
            except Exception:
                return default
        return default

    def getBlueprintAttr(self, bpRel: Optional[str], attrName: str, default: Any) -> Any:
        if isinstance(bpRel, str):
            prefix = "Data.Blueprints."
            if bpRel.startswith(prefix):
                key = bpRel[len(prefix) :].replace(".", "/")
                bpData = GameData.blueprintsData.get(key)
                if isinstance(bpData, dict):
                    attrs = bpData.get("attrs")
                    if isinstance(attrs, dict) and attrName in attrs:
                        return attrs.get(attrName, default)
        clsObj = self._resolveActorClass(bpRel)
        return self._getClassAttr(clsObj, attrName, default)

    def _getActorBlueprintAttr(self, actorData: Dict[str, Any], attrName: str, default: Any) -> Any:
        mapData = GameData.mapData.get(self.mapKey)
        if isinstance(mapData, dict):
            root = mapData.get("BPClassVarChanged")
            if isinstance(root, dict):
                tag = str(actorData.get("tag", ""))
                changes = root.get(tag)
                if isinstance(changes, dict) and attrName in changes:
                    return changes.get(attrName, default)
        return self.getBlueprintAttr(actorData.get("bp"), attrName, default)

    def _isCharacterActor(self, bpRel: Optional[str]) -> bool:
        clsObj = self._resolveActorClass(bpRel)
        if not isinstance(clsObj, type):
            return False
        try:
            Engine = System.GetModule("Engine")
            Character = Engine.Gameplay.Actors.Character
            return issubclass(clsObj, Character)
        except Exception:
            return False

    def _actorPreviewAnimatable(
        self, bpRel: Optional[str], actorData: Optional[Dict[str, Any]] = None
    ) -> bool:
        attrGetter = (
            lambda name, default: self._getActorBlueprintAttr(actorData, name, default)
            if isinstance(actorData, dict)
            else self.getBlueprintAttr(bpRel, name, default)
        )
        if not self._toBool(attrGetter("animatable", False), False):
            return False
        if self._isCharacterActor(bpRel):
            return self._toBool(attrGetter("animateWithoutMoving", False), False)
        return True

    def _actorAnimationFrame(
        self,
        bpRel: Optional[str],
        frameWidth: int,
        textureWidth: int,
        actorData: Optional[Dict[str, Any]] = None,
    ) -> int:
        frameWidth = max(1, int(frameWidth))
        textureWidth = max(1, int(textureWidth))
        frameCount = max(1, textureWidth // frameWidth)
        if frameCount <= 1:
            return 0
        intervalValue = (
            self._getActorBlueprintAttr(actorData, "switchInterval", _DEFAULT_ACTOR_SWITCH_INTERVAL)
            if isinstance(actorData, dict)
            else self.getBlueprintAttr(bpRel, "switchInterval", _DEFAULT_ACTOR_SWITCH_INTERVAL)
        )
        interval = self._toPositiveFloat(
            intervalValue,
            _DEFAULT_ACTOR_SWITCH_INTERVAL,
        )
        return int(self._actorAnimationTime / interval) % frameCount

    def _actorPreviewRect(
        self,
        bpRel: Optional[str],
        image: Optional[QtGui.QImage],
        rect: Optional[Tuple[int, int, int, int]],
        sourceTileSize: int,
        actorData: Optional[Dict[str, Any]] = None,
    ) -> Tuple[int, int, int, int]:
        if image is not None and not image.isNull() and self._isCharacterActor(bpRel):
            w = max(1, image.width() // _CHARACTER_SHEET_COLS)
            h = max(1, image.height() // _CHARACTER_SHEET_ROWS)
            direction = (
                self._getActorBlueprintAttr(actorData, "direction", 0)
                if isinstance(actorData, dict)
                else self.getBlueprintAttr(bpRel, "direction", 0)
            )
            try:
                row = max(0, min(_CHARACTER_SHEET_ROWS - 1, int(direction)))
            except (TypeError, ValueError):
                row = 0
            frame = (
                self._actorAnimationFrame(bpRel, w, image.width(), actorData)
                if self._actorPreviewAnimatable(bpRel, actorData)
                else 0
            )
            return (frame * w, row * h, w, h)
        if rect is None:
            if image is not None and not image.isNull():
                return (0, 0, image.width(), image.height())
            return (0, 0, sourceTileSize, sourceTileSize)
        sx, sy, w, h = rect
        if image is not None and not image.isNull() and self._actorPreviewAnimatable(bpRel, actorData):
            frame = self._actorAnimationFrame(bpRel, w, image.width(), actorData)
            sx = (sx + frame * w) % max(1, image.width())
        return (sx, sy, w, h)

    def _resolveTexturePath(self, texturePath: Optional[str]) -> str:
        if isinstance(texturePath, str) and texturePath.strip():
            p = texturePath.strip()
            if os.path.isabs(p):
                return p
            if p.startswith("Assets/") or p.startswith("Assets\\"):
                return os.path.join(EditorStatus.PROJ_PATH, p)
            return os.path.join(EditorStatus.PROJ_PATH, "Assets", "Characters", p)
        return ""

    def _resolveShaderPath(self, shaderPath: Optional[str]) -> str:
        if isinstance(shaderPath, str) and shaderPath.strip():
            p = shaderPath.strip()
            if os.path.isabs(p):
                return p
            if p.startswith("Assets/Shaders/") or p.startswith("Assets\\Shaders\\"):
                return os.path.join(EditorStatus.PROJ_PATH, p)
            return os.path.join(EditorStatus.PROJ_PATH, "Assets", "Shaders", p)
        return ""

    def _resolveTextureImage(self, texturePath: Optional[str]) -> Optional[QtGui.QImage]:
        path = self._resolveTexturePath(texturePath)
        if not path:
            return None
        img = QtGui.QImage(path)
        if img.isNull():
            return None
        return img

    def _getFileMtimeNs(self, path: str) -> int:
        try:
            return int(os.stat(path).st_mtime_ns)
        except OSError:
            return 0

    def _renderActorShaderImage(
        self,
        texturePath: Optional[str],
        shaderPath: Optional[str],
        rect: Optional[Tuple[int, int, int, int]],
        textureWidth: int,
        shaderTime: float = 0.0,
    ) -> Optional[QtGui.QImage]:
        textureAbs = self._resolveTexturePath(texturePath)
        shaderAbs = self._resolveShaderPath(shaderPath)
        if not textureAbs or not shaderAbs or rect is None or not SFMLRender.hasUsableShader(shaderAbs):
            return None
        hasTimeUniform = "time" in SFMLRender.getShaderUniforms(shaderAbs)
        key = (
            os.path.normcase(os.path.abspath(textureAbs)),
            os.path.normcase(os.path.abspath(shaderAbs)),
            rect,
            self._getFileMtimeNs(textureAbs),
            self._getFileMtimeNs(shaderAbs),
        )
        if not hasTimeUniform:
            cached = self._actorShaderImageCache.get(key)
            if cached is not None and not cached.isNull():
                return cached
        encoded = SFMLRender.renderTextureWithShaderToMemory(
            textureAbs,
            shaderAbs,
            rect,
            shaderTime=shaderTime,
            textureWidth=textureWidth,
        )
        if encoded is None:
            return None
        img = QtGui.QImage()
        if not img.loadFromData(encoded, "PNG"):
            return None
        if not hasTimeUniform:
            self._actorShaderImageCache[key] = img
        return img

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

    def _drawActorsForLayer(self, painter: QtGui.QPainter, layerName: str, tileSize: int, opacity: float) -> None:
        actors = self._getActorListForLayer(layerName)
        if not actors:
            return
        sourceTileSize = max(1, int(EditorStatus.CELLSIZE))
        displayScale = float(tileSize) / float(sourceTileSize)
        oldOpacity = painter.opacity()
        painter.setOpacity(opacity)
        for index, entry in enumerate(actors):
            if not isinstance(entry, dict):
                continue
            pos = entry.get("position")
            if not isinstance(pos, (list, tuple)) or len(pos) < 2:
                continue
            try:
                gx = int(pos[0])
                gy = int(pos[1])
            except Exception:
                continue

            bpRel = entry.get("bp")

            translation = self._toVec2f(
                self._getActorBlueprintAttr(entry, "defaultTranslation", (0.0, 0.0)), 0.0, 0.0
            )

            rotation = float(self._getActorBlueprintAttr(entry, "defaultRotation", 0.0))

            scaleVal = self._toVec2f(self._getActorBlueprintAttr(entry, "defaultScale", (1.0, 1.0)), 1.0, 1.0)

            origin = self._toVec2f(self._getActorBlueprintAttr(entry, "defaultOrigin", (0.0, 0.0)), 0.0, 0.0)

            texPath = self._getActorBlueprintAttr(entry, "texturePath", "")
            shaderPath = self._getActorBlueprintAttr(entry, "shaderPath", "")
            hue = self._normaliseActorHue(self._getActorBlueprintAttr(entry, "hue", 0.0))
            rectT = self._toRectTuple(self._getActorBlueprintAttr(entry, "defaultRect", None))

            px = gx * tileSize
            py = gy * tileSize

            painter.save()
            painter.translate(px, py)
            painter.scale(displayScale, displayScale)
            painter.translate(translation[0], translation[1])
            painter.rotate(rotation)
            painter.scale(scaleVal[0], scaleVal[1])

            img = self._resolveTextureImage(texPath)
            sx, sy, w, h = self._actorPreviewRect(bpRel, img, rectT, sourceTileSize, entry)
            previewRect = (sx, sy, w, h)
            shaderImg = self._renderActorShaderImage(
                texPath,
                shaderPath,
                previewRect,
                img.width() if img is not None else 0,
                self._actorAnimationTime,
            )
            if shaderImg is not None:
                drawImg = self._applyActorHueToImage(shaderImg, hue)
                src = QtCore.QRectF(0, 0, drawImg.width(), drawImg.height())
                dst = QtCore.QRectF(-origin[0], -origin[1], w, h)
                painter.drawImage(dst, drawImg, src)
            elif img is not None:
                dst = QtCore.QRectF(-origin[0], -origin[1], w, h)
                if self._isNeutralActorHue(hue):
                    src = QtCore.QRectF(sx, sy, w, h)
                    painter.drawImage(dst, img, src)
                else:
                    drawImg = self._applyActorHueToImage(img.copy(sx, sy, w, h), hue)
                    src = QtCore.QRectF(0, 0, drawImg.width(), drawImg.height())
                    painter.drawImage(dst, drawImg, src)
            else:
                color = QtGui.QColor(0, 120, 255, 160)
                r = QtCore.QRectF(-origin[0], -origin[1], w, h)
                painter.fillRect(r, color)
                painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 220), 1))
                painter.drawRect(r)

            if self.selectedActorLayerName == layerName and self.selectedActorIndex == index:
                pen = QtGui.QPen(QtGui.QColor(255, 220, 0, 255), 2)
                pen.setCosmetic(True)
                painter.setPen(pen)
                painter.setBrush(QtCore.Qt.NoBrush)
                painter.drawRect(QtCore.QRectF(-origin[0], -origin[1], w, h))

            painter.restore()

            cx = px + tileSize / 2
            cy = py + tileSize / 2
            painter.setPen(QtCore.Qt.NoPen)
            painter.setBrush(QtGui.QBrush(QtGui.QColor(0, 255, 0, 220)))
            painter.drawEllipse(QtCore.QPointF(cx, cy), 3, 3)

        painter.setOpacity(oldOpacity)

    def _stopActorMoveDrag(self) -> None:
        if not self._actorMoveDragging:
            return
        self._actorMoveDragging = False
        self._actorMoveDragLayerName = None
        self._actorMoveDragIndex = None
        self._actorMoveDragLastGrid = None
        self._actorMoveDragTitleRefreshed = False
        self.unsetCursor()

    def _makeDefaultTag(self, clsObj: Optional[type], bpRel: str, layerName: str, gx: int, gy: int) -> str:
        prefix = None
        hasBlueprintTagAttr = False
        try:
            if isinstance(bpRel, str) and bpRel.startswith("Data.Blueprints."):
                found, t = GameData._GetBlueprintDefaultAttr(bpRel, "tag", set())
                if found:
                    hasBlueprintTagAttr = True
                    if isinstance(t, str) and t.strip():
                        prefix = t.strip()
        except Exception:
            prefix = None
        try:
            if not hasBlueprintTagAttr and isinstance(clsObj, type):
                t = getattr(clsObj, "tag")
                if isinstance(t, str) and t.strip():
                    prefix = t.strip()
        except Exception:
            prefix = None
        if not isinstance(prefix, str) or not prefix:
            prefix = bpRel
        return f"{prefix}_{layerName}_{gx}_{gy}"

    def makeUniqueActorTag(
        self, tag: str, ignoreLayerName: Optional[str] = None, ignoreIndex: Optional[int] = None
    ) -> str:
        base = tag.strip() if isinstance(tag, str) else ""
        if not base:
            return ""
        candidate = base
        index = 2
        while self._actorTagExists(candidate, ignoreLayerName, ignoreIndex):
            candidate = f"{base}_{index}"
            index += 1
        return candidate

    def _makeUniqueDefaultTag(
        self,
        clsObj: Optional[type],
        bpRel: str,
        layerName: str,
        gx: int,
        gy: int,
        ignoreLayerName: Optional[str] = None,
        ignoreIndex: Optional[int] = None,
    ) -> str:
        return self.makeUniqueActorTag(
            self._makeDefaultTag(clsObj, bpRel, layerName, gx, gy), ignoreLayerName, ignoreIndex
        )

    def _actorTagExists(
        self, tag: str, ignoreLayerName: Optional[str] = None, ignoreIndex: Optional[int] = None
    ) -> bool:
        if not tag or not self.mapKey:
            return False
        mapData = GameData.mapData.get(self.mapKey)
        if not isinstance(mapData, dict):
            return False
        actorsDict = mapData.get("actors")
        if not isinstance(actorsDict, dict):
            return False
        for layerName, actors in actorsDict.items():
            if not isinstance(actors, list):
                continue
            for index, entry in enumerate(actors):
                if layerName == ignoreLayerName and index == ignoreIndex:
                    continue
                if isinstance(entry, dict) and entry.get("tag") == tag:
                    return True
        return False

    def _getActorDefaultTagBase(self, entry: Dict[str, Any], layerName: str) -> Optional[str]:
        pos = entry.get("position")
        if not isinstance(pos, (list, tuple)) or len(pos) < 2:
            return None
        try:
            gx = int(pos[0])
            gy = int(pos[1])
        except Exception:
            return None
        bpRel = entry.get("bp", "")
        clsObj = self._resolveActorClass(bpRel)
        return self._makeDefaultTag(clsObj, bpRel, layerName, gx, gy)

    def _getDefaultTagSuffix(self, tag: Optional[str], baseTag: Optional[str]) -> Optional[str]:
        if not isinstance(tag, str) or not isinstance(baseTag, str) or not baseTag:
            return None
        if tag == baseTag:
            return ""
        prefix = f"{baseTag}_"
        if tag.startswith(prefix):
            suffix = tag[len(prefix) :]
            if suffix.isdigit():
                return suffix
        return None

    def _hitTestActor(self, layerName: str, pos: QtCore.QPoint, tileSize: int) -> Optional[int]:
        actors = self._getActorListForLayer(layerName)
        if not actors:
            return None
        sourceTileSize = max(1, int(EditorStatus.CELLSIZE))
        displayScale = float(tileSize) / float(sourceTileSize)
        px = int(pos.x())
        py = int(pos.y())
        for i, entry in enumerate(actors):
            if not isinstance(entry, dict):
                continue
            p = entry.get("position")
            if not isinstance(p, (list, tuple)) or len(p) < 2:
                continue
            try:
                gx = int(p[0])
                gy = int(p[1])
            except Exception:
                continue
            bpRel = entry.get("bp")
            rectT = self._toRectTuple(self.getBlueprintAttr(bpRel, "defaultRect", None))
            img = self._resolveTextureImage(self.getBlueprintAttr(bpRel, "texturePath", ""))
            origin = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultOrigin", (0.0, 0.0)), 0.0, 0.0)
            scale = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultScale", (1.0, 1.0)), 1.0, 1.0)
            sx, sy, w, h = self._actorPreviewRect(bpRel, img, rectT, sourceTileSize)
            dw = int(w * scale[0] * displayScale)
            dh = int(h * scale[1] * displayScale)
            dx = int(gx * tileSize - origin[0] * scale[0] * displayScale)
            dy = int(gy * tileSize - origin[1] * scale[1] * displayScale)
            rect = QtCore.QRect(dx, dy, dw, dh)
            if rect.contains(px, py):
                return i
        return None

    def dragEnterEvent(self, e: QtGui.QDragEnterEvent) -> None:
        if not self.isEnabled():
            return
        if not self.acceptDrops():
            return
        mimeData = e.mimeData()
        if not mimeData:
            return
        if not mimeData.hasUrls():
            return
        urls = [u for u in mimeData.urls() if u.isLocalFile()]
        if not urls:
            return
        path = urls[0].toLocalFile()
        ext = os.path.splitext(path)[1].lower()
        if ext in DATA_FILE_EXTENSIONS:
            e.acceptProposedAction()

    def dragMoveEvent(self, e: QtGui.QDragMoveEvent) -> None:
        if not self.isEnabled():
            return
        if not self.acceptDrops():
            return
        mimeData = e.mimeData()
        if not mimeData:
            return
        if not mimeData.hasUrls():
            return
        urls = [u for u in mimeData.urls() if u.isLocalFile()]
        if not urls:
            return
        path = urls[0].toLocalFile()
        ext = os.path.splitext(path)[1].lower()
        if ext in DATA_FILE_EXTENSIONS:
            e.acceptProposedAction()

    def dropEvent(self, e: QtGui.QDropEvent) -> None:
        if not self.isEnabled():
            return
        if not self.acceptDrops():
            return
        if self.mapData is None:
            return
        if self.selectedLayerName is None:
            return
        mimeData = e.mimeData()
        if not mimeData:
            return
        if not mimeData.hasUrls():
            return
        urls = [u for u in mimeData.urls() if u.isLocalFile()]
        if not urls:
            return
        path = urls[0].toLocalFile()
        ext = os.path.splitext(path)[1].lower()
        if ext not in DATA_FILE_EXTENSIONS:
            return
        mapPos = self._mapDisplayPos(e.pos())
        gridPos = self._gridPosFromMapDisplayPos(mapPos)
        if gridPos is None:
            return
        gx, gy = gridPos
        w = self.window()
        msg = None
        data = None
        try:
            if ext == DATA_FORMAT_EXTENSIONS[DATA_FORMAT_JSON]:
                data = Utils.File.GetJSONData(path)
            else:
                data = Utils.File.LoadData(path)
        except Exception as ex:
            msg = ELOC("NOT_ACTOR_TYPE")
        okDict = isinstance(data, dict)
        if okDict and data.get("type") == "blueprint":
            parentClass = data.get("parent")
            clsObj = None
            if isinstance(parentClass, str) and parentClass.strip():
                try:
                    clsObj = GameData.classDict.get(parentClass, EditorStatus.PROJ_PATH)
                except Exception:
                    clsObj = None
            try:
                ActorBases = []
                Engine = System.GetModule("Engine")
                ActorBases.append(Engine.Gameplay.Actors.Actor)
                okSubclass = (
                    bool(clsObj) and isinstance(clsObj, type) and any(issubclass(clsObj, b) for b in ActorBases)
                )
                if okSubclass:
                    bpRel = None
                    mapsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
                    blueprintsRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Blueprints")
                    try:
                        absPath = os.path.abspath(path)
                        if absPath.startswith(os.path.abspath(blueprintsRoot) + os.sep):
                            rel = os.path.relpath(absPath, blueprintsRoot)
                            namePart, _ = os.path.splitext(rel)
                            namePart = namePart.replace("\\", "/")
                            bpRel = "Data.Blueprints." + namePart.replace("/", ".")
                    except Exception:
                        bpRel = None
                    if bpRel:
                        mainWindow = getattr(Utils.File, "mainWindow", None)
                        actorQueuePanel = getattr(mainWindow, "actorQueuePanel", None)
                        if actorQueuePanel is not None:
                            actorQueuePanel.addOrPromote(bpRel)
                        if self._placeActorBlueprintAt(bpRel, gx, gy):
                            msg = ELOC("DRAG_INFO").format(file=os.path.basename(path), x=gx, y=gy)
                        else:
                            msg = ELOC("NOT_ACTOR_TYPE")
                    else:
                        msg = ELOC("NOT_ACTOR_TYPE")
                else:
                    msg = ELOC("NOT_ACTOR_TYPE")
            except Exception:
                msg = ELOC("NOT_ACTOR_TYPE")
        else:
            msg = ELOC("NOT_ACTOR_TYPE")
        toast = getattr(w, "toast", None) if isinstance(w, QtWidgets.QWidget) else None
        if msg and isinstance(toast, Toast):
            toast.showMessage(msg, 3000)
        e.acceptProposedAction()
