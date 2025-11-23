# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
import os
from typing import Any, Dict, List, Optional, Tuple
import importlib
from PyQt5 import QtWidgets, QtGui, QtCore
import Utils
import EditorStatus
from Utils import Locale
import Data


@dataclass
class MapData:
    mapName: str
    width: int
    height: int
    layers: Dict[str, Any]


class EditorPanel(QtWidgets.QWidget):
    tileNumberPicked = QtCore.pyqtSignal(int)

    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        self.selctedPos: Tuple[int, int] = None
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.mapFilePath = ""
        self.mapData: Optional[MapData] = None
        self._pixmap: Optional[QtGui.QPixmap] = None
        self.selectedLayerName: Optional[str] = None
        self.selectedTileNumber: Optional[int] = None
        self.tileModeEnabled: bool = True
        super().__init__(parent)
        Utils.Panel.applyDisabledOpacity(self)

    def refreshMap(self, mapFileName: Optional[str] = None):
        self.selctedPos = None
        self.mapData = None
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.mapFilePath = ""
        Utils.Panel.clearPanel(self)
        if not mapFileName:
            return
        if os.path.isabs(mapFileName):
            self.mapFilePath = mapFileName
        else:
            self.mapFilePath = os.path.join(self._mapFilesRoot, mapFileName)
        mapData = Utils.File.loadData(self.mapFilePath)
        self.applyMapData(mapData)
        self._renderFromMapData()
        self._updateContentSize()
        self.update()

    def applyMapData(self, data):
        Engine: TempEngine = importlib.import_module("Engine")
        TileLayerData = Engine.Gameplay.TileLayerData
        mapName = data["mapName"]
        width = data["width"]
        height = data["height"]
        layers = data["layers"]
        mapLayers = {}
        for layerName, layerData in layers.items():
            name = layerData["layerName"]
            layerTileset = Data.GameData.tilesetData[layerData["layerTileset"]]
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
        if self.mapData is None:
            self._pixmap = None
            return
        tileSize = EditorStatus.CELLSIZE
        w = self.mapData.width * tileSize
        h = self.mapData.height * tileSize
        img = QtGui.QImage(w, h, QtGui.QImage.Format_ARGB32)
        img.fill(QtGui.QColor(0, 0, 0))
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
            columns = tileset.width() // tileSize
            for y in range(self.mapData.height):
                for x in range(self.mapData.width):
                    tileNumber = layer.tiles[y][x]
                    if tileNumber is None:
                        continue
                    tu = tileNumber % columns
                    tv = tileNumber // columns
                    src = QtCore.QRect(tu * tileSize, tv * tileSize, tileSize, tileSize)
                    dst = QtCore.QRect(x * tileSize, y * tileSize, tileSize, tileSize)
                    painter.drawImage(dst, tileset, src)
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
        self._renderFromMapData()

    def setTileMode(self, enabled: bool) -> None:
        self.tileModeEnabled = bool(enabled)

    def setSelectedTileNumber(self, num: Optional[int]) -> None:
        self.selectedTileNumber = None if num is None else int(num)

    def getLayerTilesetKey(self, name: str) -> Optional[str]:
        if self.mapData is None:
            return None
        if name not in self.mapData.layers:
            return None
        layer = self.mapData.layers[name]
        key = getattr(layer, "layerTilesetKey", None)
        if key:
            return key
        for k, ts in Data.GameData.tilesetData.items():
            if ts.fileName == layer.layerTileset.fileName:
                return k
        return None

    def setLayerTilesetForSelectedLayer(self, key: str) -> None:
        if self.mapData is None:
            return
        if self.selectedLayerName is None:
            return
        if key not in Data.GameData.tilesetData:
            return
        layer = self.mapData.layers.get(self.selectedLayerName)
        if not layer:
            return
        ts = Data.GameData.tilesetData[key]
        setattr(layer, "layerTileset", ts)
        setattr(layer, "layerTilesetKey", key)
        self._renderFromMapData()
        self.update()

    def addEmptyLayer(self, name: Optional[str] = None, filePath: str = "") -> Optional[str]:
        if self.mapData is None:
            return None
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
        keys = list(Data.GameData.tilesetData.keys()) if hasattr(Data, "GameData") else []
        ts_key = keys[0] if keys else None
        ts = Data.GameData.tilesetData.get(ts_key) if ts_key else None
        layer = TileLayerData(
            name, ts if ts is not None else Data.GameData.tilesetData[next(iter(Data.GameData.tilesetData))], tiles
        )
        setattr(layer, "layerTilesetKey", ts_key or next(iter(Data.GameData.tilesetData)))
        self.mapData.layers[name] = layer
        self._renderFromMapData()
        self.update()
        return name

    def removeLayer(self, name: str) -> bool:
        if self.mapData is None:
            return False
        if name not in self.mapData.layers:
            return False
        self.mapData.layers.pop(name, None)
        if self.selectedLayerName == name:
            self.selectedLayerName = None
        self._renderFromMapData()
        self.update()
        return True

    def paintEvent(self, e: QtGui.QPaintEvent) -> None:
        p = QtGui.QPainter(self)
        if self._pixmap is not None:
            p.drawPixmap(0, 0, self._pixmap)
        p.end()

    def changeEvent(self, e: QtCore.QEvent) -> None:
        if e.type() == QtCore.QEvent.EnabledChange:
            Utils.Panel.applyDisabledOpacity(self)
        super().changeEvent(e)

    def mousePressEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.mapData is None:
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        tileSize = EditorStatus.CELLSIZE
        gx = x // tileSize
        gy = y // tileSize
        if gx < 0 or gy < 0 or gx >= self.mapData.width or gy >= self.mapData.height:
            return
        self.selctedPos = (gx, gy)
        if not self.tileModeEnabled:
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
        layer.tiles[gy][gx] = self.selectedTileNumber
        self._renderFromMapData()
        self.update()

    def mouseMoveEvent(self, e: QtGui.QMouseEvent) -> None:
        if self.mapData is None:
            return
        x = int(e.pos().x())
        y = int(e.pos().y())
        tileSize = EditorStatus.CELLSIZE
        gx = x // tileSize
        gy = y // tileSize
        if gx < 0 or gy < 0 or gx >= self.mapData.width or gy >= self.mapData.height:
            return
        self.selctedPos = (gx, gy)
        if not self.tileModeEnabled:
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
            dataDict = {}
            dataDict["mapName"] = self.mapData.mapName
            dataDict["width"] = self.mapData.width
            dataDict["height"] = self.mapData.height
            dataDict["layers"] = {}
            for layerName, layerData in self.mapData.layers.items():
                data = {}
                data["layerName"] = layerData.layerName
                tileset = layerData.layerTileset
                data["layerTileset"] = next(
                    (key for key, value in Data.GameData.tilesetData.items() if value == tileset), None
                )
                data["tiles"] = layerData.tiles
                data["actors"] = []
                dataDict["layers"][layerName] = data
            Utils.File.saveData(self.mapFilePath, dataDict)
        except Exception as e:
            return False, str(e)
        return True, Locale.getContent("SAVE_PATH").format(self.mapFilePath)
