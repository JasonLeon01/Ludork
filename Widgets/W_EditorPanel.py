# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
import os
import copy
from typing import Any, Dict, List, Optional, Tuple
import importlib
from PyQt5 import QtWidgets, QtGui, QtCore
import Utils
import EditorStatus
from Utils import Locale, System
from Data import GameData


@dataclass
class MapData:
    mapName: str
    width: int
    height: int
    layers: Dict[str, Any]


class EditorPanel(QtWidgets.QWidget):
    tileNumberPicked = QtCore.pyqtSignal(int)
    dataChanged = QtCore.pyqtSignal()
    lightSelectionChanged = QtCore.pyqtSignal(str, object, object)
    lightDataChanged = QtCore.pyqtSignal(str, object, object)
    actorSelectionChanged = QtCore.pyqtSignal(str, object, object)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        self.selctedPos: Tuple[int, int] = None
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.mapFilePath = ""
        self.mapKey: str = ""
        self.mapData: Optional[MapData] = None
        self._pixmap: Optional[QtGui.QPixmap] = None
        self._cachedTilesetImage: Optional[QtGui.QImage] = None
        self.selectedLayerName: Optional[str] = None
        self.selectedTileNumber: Optional[int] = None
        self.tileModeEnabled: bool = True
        self.rectStartPos: Optional[Tuple[int, int]] = None
        self.selectedLightIndex: Optional[int] = None
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
        super().__init__(parent)
        self.setMouseTracking(True)
        self.setFocusPolicy(QtCore.Qt.ClickFocus)
        self.setAcceptDrops(False)
        Utils.Panel.applyDisabledOpacity(self)

    def _updateCachedTileset(self) -> None:
        self._cachedTilesetImage = None
        if self.mapData is None or self.selectedLayerName is None:
            return
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        ts_path = os.path.join(
            EditorStatus.PROJ_PATH,
            "Assets",
            "Tilesets",
            layer.layerTileset.fileName,
        )
        if os.path.exists(ts_path):
            self._cachedTilesetImage = QtGui.QImage(ts_path)

    def refreshMap(self, mapFileName: Optional[str] = None):
        self.selctedPos = None
        self.mapData = None
        self._pixmap = None
        self._setSelectedLightIndex(None)
        self.actorSelectionChanged.emit(None, None, None)
        self.setMinimumSize(0, 0)
        self.resize(0, 0)
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.mapFilePath = ""
        Utils.Panel.clearPanel(self)
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
            mapData = Utils.File.loadData(self.mapFilePath)
            GameData.mapData[self.mapKey] = mapData
        self.applyMapData(mapData)
        self._updateCachedTileset()
        self._renderFromMapData()
        self._updateContentSize()
        self.update()

    def applyMapData(self, data):
        Engine = System.getModule("Engine")
        TileLayerData = Engine.Gameplay.TileLayerData
        mapName = data["mapName"]
        width = data["width"]
        height = data["height"]
        layers = data["layers"]
        mapLayers = {}
        for layerName, layerData in layers.items():
            name = layerData["layerName"]
            layerTileset = GameData.tilesetData[layerData["layerTileset"]]
            layerTiles = layerData["tiles"]
            tiles: List[List[Tile]] = []
            for y in range(height):
                tiles.append([])
                for x in range(width):
                    tiles[-1].append(layerTiles[y][x])
            layer = TileLayerData(name, layerTileset, tiles)
            setattr(layer, "layerTilesetKey", layerData["layerTileset"])
            mapLayers[layerName] = layer
        self.mapData = MapData(mapName, width, height, mapLayers)

    def _renderFromMapData(self) -> None:
        try:
            from EditorExtensions.EditorExtension import C_RenderTilemapRGBA

            loadedExt = True
        except ImportError as e:
            loadedExt = False
            print(f"Failed to load EditorExtension, try to render map with python. Error: {e}")
        if self.mapData is None:
            self._pixmap = None
            return
        tileSize = EditorStatus.CELLSIZE
        w = self.mapData.width * tileSize
        h = self.mapData.height * tileSize
        img = QtGui.QImage(w, h, QtGui.QImage.Format_ARGB32)
        img.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(img)
        sel = self.selectedLayerName
        for layerName, layer in self.mapData.layers.items():
            if not getattr(layer, "visible", True):
                continue
            painter.setOpacity(1.0 if (sel is None or layerName == sel) else 0.5)
            ts_path = os.path.join(
                EditorStatus.PROJ_PATH,
                "Assets",
                "Tilesets",
                layer.layerTileset.fileName,
            )
            tileset = QtGui.QImage(ts_path)
            if tileset.isNull():
                continue
            if loadedExt:
                try:
                    tilesetRgba = tileset.convertToFormat(QtGui.QImage.Format_RGBA8888)
                    tsW = tilesetRgba.width()
                    tsH = tilesetRgba.height()
                    tsStride = tilesetRgba.bytesPerLine()
                    tsBytes = tilesetRgba.bits().asstring(tsH * tsStride)
                    from array import array

                    tilesFlat = []
                    for y in range(self.mapData.height):
                        for x in range(self.mapData.width):
                            v = layer.tiles[y][x]
                            tilesFlat.append(-1 if v is None else int(v))
                    tilesBuf = memoryview(array("i", tilesFlat))
                    data = C_RenderTilemapRGBA(
                        memoryview(tsBytes),
                        tsW,
                        tsH,
                        tsStride,
                        tilesBuf,
                        self.mapData.width,
                        self.mapData.height,
                        tileSize,
                    )
                    layerImg = QtGui.QImage(
                        data,
                        self.mapData.width * tileSize,
                        self.mapData.height * tileSize,
                        QtGui.QImage.Format_RGBA8888,
                    )
                    layerImg = layerImg.copy()
                    painter.drawImage(0, 0, layerImg)
                except Exception as e:
                    print(f"Failed to render tilemap by C extension: {e}")
                    columns = tileset.width() // tileSize
                    rows = tileset.height() // tileSize
                    total = columns * rows
                    for y in range(self.mapData.height):
                        for x in range(self.mapData.width):
                            tileNumber = layer.tiles[y][x]
                            if tileNumber is None:
                                continue
                            n = int(tileNumber)
                            if n < 0 or n >= total:
                                continue
                            tu = n % columns
                            tv = n // columns
                            src = QtCore.QRect(tu * tileSize, tv * tileSize, tileSize, tileSize)
                            dst = QtCore.QRect(x * tileSize, y * tileSize, tileSize, tileSize)
                            painter.drawImage(dst, tileset, src)
                painter.drawImage(0, 0, layerImg)
            else:
                columns = tileset.width() // tileSize
                rows = tileset.height() // tileSize
                for y in range(self.mapData.height):
                    for x in range(self.mapData.width):
                        tileNumber = layer.tiles[y][x]
                        if tileNumber is None:
                            continue
                        n = int(tileNumber)
                        if n < 0 or n >= total:
                            continue
                        tu = n % columns
                        tv = n // columns
                        src = QtCore.QRect(tu * tileSize, tv * tileSize, tileSize, tileSize)
                        dst = QtCore.QRect(x * tileSize, y * tileSize, tileSize, tileSize)
                        painter.drawImage(dst, tileset, src)
            self._drawActorsForLayer(painter, layerName, tileSize, 1.0 if (sel is None or layerName == sel) else 0.5)
        painter.end()
        self._pixmap = QtGui.QPixmap.fromImage(img)
        self.update()

    def _updateContentSize(self) -> None:
        if self.mapData is None:
            self.setMinimumSize(0, 0)
            return
        tileSize = EditorStatus.CELLSIZE
        w = int(self.mapData.width * tileSize)
        h = int(self.mapData.height * tileSize)
        self.setMinimumSize(w, h)
        self.resize(w, h)

    def getLayerNames(self) -> List[str]:
        if self.mapData is None:
            return []
        return list(self.mapData.layers.keys())

    def setSelectedLayer(self, name: Optional[str]) -> None:
        self.selectedLayerName = name
        self._updateCachedTileset()
        self._renderFromMapData()

    def setTileMode(self, enabled: bool) -> None:
        self.tileModeEnabled = bool(enabled)
        if self.tileModeEnabled:
            self._stopLightRadiusDrag()
            self._stopLightMoveDrag()
            self._setSelectedLightIndex(None)
            self._setLightOverlayEnabled(False)
            self.actorSelectionChanged.emit(None, None, None)
        self.update()

    def setLightOverlayEnabled(self, enabled: bool) -> None:
        self._setLightOverlayEnabled(bool(enabled))
        self.update()

    def _setLightOverlayEnabled(self, enabled: bool) -> None:
        enabled = bool(enabled)
        if self._lightOverlayEnabled == enabled:
            return
        self._lightOverlayEnabled = enabled
        if not enabled:
            self._stopLightRadiusDrag()
            self._stopLightMoveDrag()
            self._setSelectedLightIndex(None)

    def setSelectedTileNumber(self, num: Optional[int]) -> None:
        self.selectedTileNumber = None if num is None else int(num)

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
        self.lightDataChanged.emit(self.mapKey, index, light)

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
        self.lightDataChanged.emit(self.mapKey, index, light)

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
        self.lightSelectionChanged.emit(self.mapKey, index, light)

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
        key = getattr(layer, "layerTilesetKey", None)
        if key:
            return key
        for k, ts in GameData.tilesetData.items():
            if ts.fileName == layer.layerTileset.fileName:
                return k
        return None

    def setLayerTilesetForSelectedLayer(self, key: str) -> None:
        if self.mapData is None:
            return
        if self.selectedLayerName is None:
            return
        if key not in GameData.tilesetData:
            return
        GameData.recordSnapshot()
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        ts = GameData.tilesetData[key]
        setattr(layer, "layerTileset", ts)
        setattr(layer, "layerTilesetKey", key)
        if self.mapKey and self.selectedLayerName in GameData.mapData.get(self.mapKey, {}).get("layers", {}):
            GameData.mapData[self.mapKey]["layers"][self.selectedLayerName]["layerTileset"] = key
        self._refreshTitle()
        self._updateCachedTileset()
        self._renderFromMapData()
        self.update()

    def addEmptyLayer(self, name: Optional[str] = None, filePath: str = "") -> Optional[str]:
        if self.mapData is None:
            return None
        GameData.recordSnapshot()
        Engine: TempEngine = importlib.import_module("Engine")
        TileLayerData = Engine.Gameplay.TileLayerData
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
        keys = list(GameData.tilesetData.keys())
        ts_key = keys[0] if keys else None
        ts = GameData.tilesetData.get(ts_key) if ts_key else None
        layer = TileLayerData(
            name, ts if ts is not None else GameData.tilesetData[next(iter(GameData.tilesetData))], tiles
        )
        setattr(layer, "layerTilesetKey", ts_key or next(iter(GameData.tilesetData)))
        self.mapData.layers[name] = layer
        if self.mapKey:
            if self.mapKey in GameData.mapData:
                if "layers" in GameData.mapData[self.mapKey]:
                    GameData.mapData[self.mapKey]["layers"][name] = {
                        "layerName": name,
                        "layerTileset": ts_key or next(iter(GameData.tilesetData)),
                        "tiles": tiles,
                        "actors": [],
                    }
        self._refreshTitle()
        self._renderFromMapData()
        self.update()
        return name

    def removeLayer(self, name: str) -> bool:
        if self.mapData is None:
            return False
        if name not in self.mapData.layers:
            return False
        GameData.recordSnapshot()
        self.mapData.layers.pop(name, None)
        if self.mapKey and self.mapKey in GameData.mapData:
            GameData.mapData[self.mapKey].get("layers", {}).pop(name, None)
        self._refreshTitle()
        if self.selectedLayerName == name:
            self.selectedLayerName = None
            self._updateCachedTileset()
        self._renderFromMapData()
        self.update()
        return True

    def reorderLayers(self, new_order: List[str]) -> None:
        if self.mapData is None:
            return
        GameData.recordSnapshot()
        new_layers = {name: self.mapData.layers[name] for name in new_order}
        self.mapData.layers = new_layers
        if self.mapKey and self.mapKey in GameData.mapData:
            game_layers = GameData.mapData[self.mapKey].get("layers", {})
            new_game_layers = {name: game_layers[name] for name in new_order}
            GameData.mapData[self.mapKey]["layers"] = new_game_layers
        self._refreshTitle()
        self._renderFromMapData()
        self.update()

    def _commitRectangle(self, endPos: Optional[Tuple[int, int]]) -> None:
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

        GameData.recordSnapshot()
        changed = False
        for y in range(min_y, max_y + 1):
            for x in range(min_x, max_x + 1):
                if layer.tiles[y][x] != self.selectedTileNumber:
                    layer.tiles[y][x] = self.selectedTileNumber
                    if self.mapKey and self.selectedLayerName:
                        if self.mapKey in GameData.mapData:
                            if self.selectedLayerName in GameData.mapData[self.mapKey].get("layers", {}):
                                GameData.mapData[self.mapKey]["layers"][self.selectedLayerName]["tiles"][y][x] = (
                                    None if self.selectedTileNumber is None else int(self.selectedTileNumber)
                                )
                    changed = True

        if changed:
            self._refreshTitle()
            self._renderFromMapData()

        self.rectStartPos = None
        self.update()

    def mouseReleaseEvent(self, e: QtGui.QMouseEvent) -> None:
        if self._lightOverlayEnabled and e.button() == QtCore.Qt.LeftButton:
            self._stopLightRadiusDrag()
            self._stopLightMoveDrag()
            self._stopActorMoveDrag()
            self.update()
            super().mouseReleaseEvent(e)
            return
        if e.button() == QtCore.Qt.LeftButton and self.rectStartPos is not None:
            self._commitRectangle(self.selctedPos)
        if e.button() == QtCore.Qt.LeftButton and self._actorMoveDragging:
            self._stopActorMoveDrag()
        super().mouseReleaseEvent(e)

    def keyReleaseEvent(self, e: QtGui.QKeyEvent) -> None:
        if e.key() == QtCore.Qt.Key_Shift and self.rectStartPos is not None:
            self._commitRectangle(self.selctedPos)
        super().keyReleaseEvent(e)

    def paintEvent(self, e: QtGui.QPaintEvent) -> None:
        p = QtGui.QPainter(self)
        r = self.rect()
        s = 16
        c1 = QtGui.QColor(220, 220, 220)
        c2 = QtGui.QColor(180, 180, 180)
        y = 0
        while y < r.height():
            x = 0
            while x < r.width():
                c = c1 if (((x // s) + (y // s)) % 2 == 0) else c2
                p.fillRect(QtCore.QRect(x, y, s, s), c)
                x += s
            y += s
        if self._pixmap is not None:
            p.drawPixmap(0, 0, self._pixmap)

        if self._lightOverlayEnabled:
            p.setRenderHint(QtGui.QPainter.Antialiasing, True)
            lights = self._getLights()
            for i, light in enumerate(lights):
                if not isinstance(light, dict):
                    continue
                pos = light.get("position")
                if not isinstance(pos, (list, tuple)) or len(pos) < 2:
                    continue
                try:
                    cx = float(pos[0])
                    cy = float(pos[1])
                    radius = float(light.get("radius", 0.0))
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

        if self.selctedPos is not None and self.selectedLayerName is not None:
            gx, gy = self.selctedPos
            tileSize = EditorStatus.CELLSIZE

            if self.rectStartPos is not None:
                sx, sy = self.rectStartPos
                min_x, max_x = min(sx, gx), max(sx, gx)
                min_y, max_y = min(sy, gy), max(sy, gy)
            else:
                min_x, max_x = gx, gx
                min_y, max_y = gy, gy

            if (
                self.tileModeEnabled
                and self.selectedTileNumber is not None
                and self._cachedTilesetImage is not None
                and not self._cachedTilesetImage.isNull()
            ):
                n = int(self.selectedTileNumber)
                columns = self._cachedTilesetImage.width() // tileSize
                rows = self._cachedTilesetImage.height() // tileSize
                total = columns * rows
                if 0 <= n < total:
                    tu = n % columns
                    tv = n // columns
                    src = QtCore.QRect(tu * tileSize, tv * tileSize, tileSize, tileSize)

                    p.setOpacity(0.5)
                    for y_idx in range(min_y, max_y + 1):
                        for x_idx in range(min_x, max_x + 1):
                            dst = QtCore.QRect(x_idx * tileSize, y_idx * tileSize, tileSize, tileSize)
                            p.drawImage(dst, self._cachedTilesetImage, src)
                    p.setOpacity(1.0)

            p.setPen(QtGui.QPen(QtCore.Qt.black, 1))
            p.setBrush(QtCore.Qt.NoBrush)

            rect_w = (max_x - min_x + 1) * tileSize
            rect_h = (max_y - min_y + 1) * tileSize
            p.drawRect(min_x * tileSize, min_y * tileSize, rect_w - 1, rect_h - 1)

        p.end()

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Utils.Panel.applyDisabledOpacity(self)
        super().changeEvent(e)

    def leaveEvent(self, a0: QtCore.QEvent) -> None:
        self._stopLightRadiusDrag()
        self._stopLightMoveDrag()
        self.selctedPos = None
        self.update()
        super().leaveEvent(a0)

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
            self._actorClipboard = copy.deepcopy(actors[index])

    def _pasteActor(self, gx: int, gy: int) -> None:
        if self._actorClipboard is None or self.selectedLayerName is None:
            return
        if not self.mapKey or self.mapKey not in GameData.mapData:
            return

        GameData.recordSnapshot()
        newActor = copy.deepcopy(self._actorClipboard)
        newActor["position"] = [gx, gy]

        bpRel = newActor.get("bp", "")
        clsObj = self._resolveActorClass(bpRel)
        newTag = self._makeDefaultTag(clsObj, bpRel, self.selectedLayerName, gx, gy)
        newActor["tag"] = newTag

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
        self._refreshTitle()
        self.dataChanged.emit()
        self._renderFromMapData()
        self.update()

    def _deleteActor(self, layerName: str, index: int) -> None:
        if not self.mapKey or self.mapKey not in GameData.mapData:
            return
        m = GameData.mapData[self.mapKey]
        actorsDict = m.get("actors", {})
        layerList = actorsDict.get(layerName)

        if isinstance(layerList, list) and 0 <= index < len(layerList):
            GameData.recordSnapshot()
            layerList.pop(index)
            self._refreshTitle()
            self.dataChanged.emit()
            self.actorSelectionChanged.emit(None, None, None)
            self._renderFromMapData()
            self.update()

    def mousePressEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.mapData is None:
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        if self._lightOverlayEnabled:
            if e.button() != QtCore.Qt.LeftButton:
                return

            idx = self.selectedLightIndex if isinstance(self.selectedLightIndex, int) else None
            if isinstance(idx, int) and self._isNearLightEdge(e.pos(), idx):
                lights = self._getLights()
                if 0 <= idx < len(lights) and isinstance(lights[idx], dict):
                    cr = self._getLightCenterRadius(lights[idx])
                    if cr is not None:
                        cx, cy, r = cr
                        GameData.recordSnapshot()
                        self._lightRadiusDragging = True
                        self._lightRadiusDragMapKey = self.mapKey
                        self._lightRadiusDragIndex = idx
                        self._lightRadiusDragCenter = (cx, cy)
                        self._lightRadiusDragLastRadius = r
                        self._lightRadiusDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return

            if isinstance(idx, int) and self._isInLightDisk(e.pos(), idx):
                lights = self._getLights()
                if 0 <= idx < len(lights) and isinstance(lights[idx], dict):
                    cr = self._getLightCenterRadius(lights[idx])
                    if cr is not None:
                        cx, cy, _ = cr
                        GameData.recordSnapshot()
                        self._lightMoveDragging = True
                        self._lightMoveDragMapKey = self.mapKey
                        self._lightMoveDragIndex = idx
                        self._lightMoveDragOffset = (float(e.pos().x()) - cx, float(e.pos().y()) - cy)
                        self._lightMoveDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return

            hit = self._hitTestLight(e.pos())
            self._setSelectedLightIndex(hit)
            if isinstance(hit, int) and self._isInLightDisk(e.pos(), hit) and not self._isNearLightEdge(e.pos(), hit):
                lights = self._getLights()
                if 0 <= hit < len(lights) and isinstance(lights[hit], dict):
                    cr = self._getLightCenterRadius(lights[hit])
                    if cr is not None:
                        cx, cy, _ = cr
                        GameData.recordSnapshot()
                        self._lightMoveDragging = True
                        self._lightMoveDragMapKey = self.mapKey
                        self._lightMoveDragIndex = hit
                        self._lightMoveDragOffset = (float(e.pos().x()) - cx, float(e.pos().y()) - cy)
                        self._lightMoveDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return
            self.update()
            return
        tileSize = EditorStatus.CELLSIZE
        gx = x // tileSize
        gy = y // tileSize
        if gx < 0 or gy < 0 or gx >= self.mapData.width or gy >= self.mapData.height:
            return
        self.selctedPos = (gx, gy)
        if not self.tileModeEnabled:
            if self.selectedLayerName is not None:
                hit = self._hitTestActor(self.selectedLayerName, e.pos(), tileSize)

                if e.button() == QtCore.Qt.RightButton:
                    menu = QtWidgets.QMenu(self)

                    actCopy = QtWidgets.QAction(Locale.getContent("COPY"), self)
                    actCopy.setEnabled(isinstance(hit, int))
                    actCopy.triggered.connect(lambda: self._copyActor(self.selectedLayerName, hit))
                    menu.addAction(actCopy)

                    actPaste = QtWidgets.QAction(Locale.getContent("PASTE"), self)
                    hasActor = self._hasActorAt(self.selectedLayerName, gx, gy)
                    canPaste = (self._actorClipboard is not None) and (hit is None) and (not hasActor)
                    actPaste.setEnabled(canPaste)
                    actPaste.triggered.connect(lambda: self._pasteActor(gx, gy))
                    menu.addAction(actPaste)

                    actDelete = QtWidgets.QAction(Locale.getContent("DELETE"), self)
                    actDelete.setEnabled(isinstance(hit, int))
                    actDelete.triggered.connect(lambda: self._deleteActor(self.selectedLayerName, hit))
                    menu.addAction(actDelete)

                    menu.exec_(e.globalPos())
                    return

                if e.button() == QtCore.Qt.LeftButton:
                    if isinstance(hit, int):
                        actors = self._getActorListForLayer(self.selectedLayerName)
                        if 0 <= hit < len(actors):
                            self.actorSelectionChanged.emit(self.selectedLayerName, hit, actors[hit])

                        GameData.recordSnapshot()
                        self._actorMoveDragging = True
                        self._actorMoveDragLayerName = self.selectedLayerName
                        self._actorMoveDragIndex = hit
                        self._actorMoveDragLastGrid = (gx, gy)
                        self._actorMoveDragTitleRefreshed = False
                        self.setCursor(QtCore.Qt.SizeAllCursor)
                        return
                    else:
                        self.actorSelectionChanged.emit(None, None, None)
            return
        if self.selectedLayerName is None:
            return
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        if e.button() == QtCore.Qt.RightButton:
            try:
                tn = layer.tiles[gy][gx]
                self.tileNumberPicked.emit(-1 if tn is None else int(tn))
            except Exception:
                self.tileNumberPicked.emit(-1)
            return
        if e.button() != QtCore.Qt.LeftButton:
            return

        if e.modifiers() & QtCore.Qt.ShiftModifier:
            self.rectStartPos = (gx, gy)
            return

        GameData.recordSnapshot()
        layer.tiles[gy][gx] = self.selectedTileNumber
        if self.mapKey and self.selectedLayerName:
            if self.mapKey in GameData.mapData:
                if self.selectedLayerName in GameData.mapData[self.mapKey].get("layers", {}):
                    GameData.mapData[self.mapKey]["layers"][self.selectedLayerName]["tiles"][gy][gx] = (
                        None if self.selectedTileNumber is None else int(self.selectedTileNumber)
                    )
        self._refreshTitle()
        self._renderFromMapData()
        self.update()

    def mouseMoveEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.mapData is None:
            return
        if self._lightOverlayEnabled:
            if self._lightMoveDragging:
                idx = self._lightMoveDragIndex
                offset = self._lightMoveDragOffset
                if isinstance(idx, int) and isinstance(offset, tuple) and len(offset) == 2:
                    ox, oy = float(offset[0]), float(offset[1])
                    x = float(e.pos().x()) - ox
                    y = float(e.pos().y()) - oy
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
                    dx = float(e.pos().x()) - cx
                    dy = float(e.pos().y()) - cy
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

            if isinstance(self.selectedLightIndex, int) and self._isNearLightEdge(e.pos(), self.selectedLightIndex):
                self.setCursor(QtCore.Qt.SizeAllCursor)
            else:
                self.unsetCursor()
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        tileSize = EditorStatus.CELLSIZE
        gx = x // tileSize
        gy = y // tileSize
        if gx < 0 or gy < 0 or gx >= self.mapData.width or gy >= self.mapData.height:
            if self.selctedPos is not None:
                self.selctedPos = None
                self.update()
            return

        if self.selctedPos != (gx, gy):
            self.selctedPos = (gx, gy)
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
                                            if isinstance(entry.get("tag"), str) and entry["tag"] == defOld:
                                                entry["tag"] = self._makeDefaultTag(
                                                    clsObj, bpRel, self._actorMoveDragLayerName, gx, gy
                                                )
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
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        try:
            if layer.tiles[gy][gx] != self.selectedTileNumber:
                layer.tiles[gy][gx] = self.selectedTileNumber
                if self.mapKey and self.selectedLayerName:
                    if self.mapKey in GameData.mapData:
                        if self.selectedLayerName in GameData.mapData[self.mapKey].get("layers", {}):
                            GameData.mapData[self.mapKey]["layers"][self.selectedLayerName]["tiles"][gy][gx] = (
                                None if self.selectedTileNumber is None else int(self.selectedTileNumber)
                            )
                self._refreshTitle()
        except Exception:
            return
        self._renderFromMapData()
        self.update()

    def saveFile(self) -> bool:
        if self.mapData is None:
            return False, Locale.getContent("MAP_DATA_NONE")
        if self.mapFilePath is None:
            return False, Locale.getContent("MAP_FILE_NONE")
        try:
            if self.mapKey and self.mapKey in GameData.mapData:
                Utils.File.saveData(self.mapFilePath, GameData.mapData[self.mapKey])
            else:
                return False, Locale.getContent("MAP_DATA_NONE")
        except Exception as e:
            return False, str(e)
        return True, Locale.getContent("SAVE_PATH").format(self.mapFilePath)

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
        GameData.recordSnapshot()
        layer = self.mapData.layers.pop(old)
        setattr(layer, "layerName", new)
        self.mapData.layers[new] = layer
        if self.mapKey and self.mapKey in GameData.mapData:
            layersDict = GameData.mapData[self.mapKey].get("layers", {})
            if old in layersDict:
                data = layersDict.pop(old)
                data["layerName"] = new
                layersDict[new] = data
        if self.selectedLayerName == old:
            self.selectedLayerName = new
        self._refreshTitle()
        self._renderFromMapData()
        self.update()
        return True

    def _refreshTitle(self) -> None:
        try:
            from Utils import System

            w = self.window()
            w.setWindowTitle(System.getTitle())
            self.dataChanged.emit()
        except Exception as e:
            print(f"Error while refreshing title: {e}")

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

    def _resolveActorClass(self, bpRel: Any) -> Optional[type]:
        if not isinstance(bpRel, str) or not bpRel.strip():
            return None
        try:
            return GameData.classDict.get(bpRel, EditorStatus.PROJ_PATH)
        except Exception:
            return None

    def _getClassAttr(self, cls: Any, name: str, default: Any) -> Any:
        if hasattr(cls, name):
            return getattr(cls, name)
        return default

    def getBlueprintAttr(self, bpRel: Any, attrName: str, default: Any) -> Any:
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

    def _resolveTextureImage(self, texturePath: Any) -> Optional[QtGui.QImage]:
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

    def _drawActorsForLayer(self, painter: QtGui.QPainter, layerName: str, tileSize: int, opacity: float) -> None:
        actors = self._getActorListForLayer(layerName)
        if not actors:
            return
        oldOpacity = painter.opacity()
        painter.setOpacity(opacity)
        for entry in actors:
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

            defTrans = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultTranslation", (0.0, 0.0)), 0.0, 0.0)
            translation = self._toVec2f(entry.get("translation", defTrans), defTrans[0], defTrans[1])

            defRot = self.getBlueprintAttr(bpRel, "defaultRotation", 0.0)
            rotation = float(entry.get("rotation", defRot))

            defScale = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultScale", (1.0, 1.0)), 1.0, 1.0)
            scaleVal = self._toVec2f(entry.get("scale", defScale), defScale[0], defScale[1])

            defOrigin = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultOrigin", (0.0, 0.0)), 0.0, 0.0)
            origin = self._toVec2f(entry.get("origin", defOrigin), defOrigin[0], defOrigin[1])

            texPath = self.getBlueprintAttr(bpRel, "texturePath", "")
            rectT = self._toRectTuple(self.getBlueprintAttr(bpRel, "defaultRect", None))

            px = gx * tileSize
            py = gy * tileSize

            painter.save()
            painter.translate(px, py)
            painter.translate(translation[0], translation[1])
            painter.rotate(rotation)
            painter.scale(scaleVal[0], scaleVal[1])

            w = tileSize
            h = tileSize
            sx = 0
            sy = 0
            if rectT:
                sx, sy, w, h = rectT

            img = self._resolveTextureImage(texPath)
            if img is not None and rectT is not None:
                src = QtCore.QRectF(sx, sy, w, h)
                dst = QtCore.QRectF(-origin[0], -origin[1], w, h)
                painter.drawImage(dst, img, src)
            else:
                color = QtGui.QColor(0, 120, 255, 160)
                r = QtCore.QRectF(-origin[0], -origin[1], w, h)
                painter.fillRect(r, color)
                painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 220), 1))
                painter.drawRect(r)

            painter.restore()

            # Draw logical grid center
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

    def _makeDefaultTag(self, clsObj: Any, bpRel: str, layerName: str, gx: int, gy: int) -> str:
        prefix = None
        try:
            if hasattr(clsObj, "tag"):
                t = getattr(clsObj, "tag")
                if isinstance(t, str) and t.strip():
                    prefix = t.strip()
        except Exception:
            prefix = None
        if not isinstance(prefix, str) or not prefix:
            prefix = bpRel
        return f"{prefix}_{layerName}_{gx}_{gy}"

    def _hitTestActor(self, layerName: str, pos: QtCore.QPoint, tileSize: int) -> Optional[int]:
        actors = self._getActorListForLayer(layerName)
        if not actors:
            return None
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
            origin = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultOrigin", (0.0, 0.0)), 0.0, 0.0)
            scale = self._toVec2f(self.getBlueprintAttr(bpRel, "defaultScale", (1.0, 1.0)), 1.0, 1.0)
            w = tileSize
            h = tileSize
            sx = 0
            sy = 0
            if rectT:
                sx, sy, w, h = rectT
            dw = int(w * scale[0])
            dh = int(h * scale[1])
            dx = int(gx * tileSize - origin[0] * scale[0])
            dy = int(gy * tileSize - origin[1] * scale[1])
            rect = QtCore.QRect(dx, dy, dw, dh)
            if rect.contains(px, py):
                return i
        return None

    def dragEnterEvent(self, e: QtGui.QDragEnterEvent) -> None:
        if not self.isEnabled():
            return
        if not self.acceptDrops():
            return
        if not e.mimeData().hasUrls():
            return
        urls = [u for u in e.mimeData().urls() if u.isLocalFile()]
        if not urls:
            return
        path = urls[0].toLocalFile()
        ext = os.path.splitext(path)[1].lower()
        if ext in (".json", ".dat"):
            e.acceptProposedAction()

    def dragMoveEvent(self, e: QtGui.QDragMoveEvent) -> None:
        if not self.isEnabled():
            return
        if not self.acceptDrops():
            return
        if not e.mimeData().hasUrls():
            return
        urls = [u for u in e.mimeData().urls() if u.isLocalFile()]
        if not urls:
            return
        path = urls[0].toLocalFile()
        ext = os.path.splitext(path)[1].lower()
        if ext in (".json", ".dat"):
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
        if not e.mimeData().hasUrls():
            return
        urls = [u for u in e.mimeData().urls() if u.isLocalFile()]
        if not urls:
            return
        path = urls[0].toLocalFile()
        ext = os.path.splitext(path)[1].lower()
        if ext not in (".json", ".dat"):
            return
        pos = e.pos()
        tileSize = EditorStatus.CELLSIZE
        gx = int(pos.x()) // tileSize
        gy = int(pos.y()) // tileSize
        if gx < 0 or gy < 0 or gx >= self.mapData.width or gy >= self.mapData.height:
            return
        w = self.window()
        msg = None
        data = None
        try:
            if ext == ".json":
                data = Utils.File.getJSONData(path)
            else:
                data = Utils.File.loadData(path)
        except Exception as e:
            msg = Locale.getContent("NOT_ACTOR_TYPE")
        okDict = isinstance(data, dict)
        bpPath = None
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
                Engine = System.getModule("Engine")
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
                        if self.mapKey and self.mapKey in GameData.mapData:
                            GameData.recordSnapshot()
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
                            tagstr = ""
                            if (
                                hasattr(clsObj, "tag")
                                and not getattr(clsObj, "tag") is None
                                and getattr(clsObj, "tag") != ""
                            ):
                                tagStr = f"{getattr(clsObj, 'tag')}_{layerKey}_{gx}_{gy}"
                            else:
                                tagStr = f"{bpRel}_{layerKey}_{gx}_{gy}"
                            actorEntry = {
                                "tag": tagStr,
                                "bp": bpRel,
                                "position": [gx, gy],
                            }
                            layerList.append(actorEntry)
                            self._refreshTitle()
                            self.dataChanged.emit()
                            self._renderFromMapData()
                            self.update()
                        msg = Locale.getContent("DRAG_INFO").format(file=os.path.basename(path), x=gx, y=gy)
                    else:
                        msg = Locale.getContent("NOT_ACTOR_TYPE")
                else:
                    msg = Locale.getContent("NOT_ACTOR_TYPE")
            except Exception:
                msg = Locale.getContent("NOT_ACTOR_TYPE")
        else:
            msg = Locale.getContent("NOT_ACTOR_TYPE")
        if msg and hasattr(w, "toast") and w.toast:
            w.toast.showMessage(msg, 3000)
        e.acceptProposedAction()
