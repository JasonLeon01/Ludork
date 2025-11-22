# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
import os
import sys
from typing import Dict, List, Optional, Tuple, TYPE_CHECKING
import importlib
from PyQt5 import QtWidgets, QtGui, QtCore
import Utils
import EditorStatus
import Data


if TYPE_CHECKING:
    import Sample.Engine as TempEngine
    from Sample.Engine.Gameplay import Tileset
    from Sample.Engine.Gameplay import TileLayerData


@dataclass
class MapData:
    mapName: str
    width: int
    height: int
    layers: Dict[str, TileLayerData]


class EditorPanel(QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        self.selctedPos: Tuple[int, int] = None
        self._mapFilesRoot = os.path.join(EditorStatus.PROJ_PATH, "Data", "Maps")
        self.mapFilePath = ""
        self.mapData: Optional[MapData] = None
        self._pixmap: Optional[QtGui.QPixmap] = None
        self.selectedLayerName: Optional[str] = None
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
            if sel is None:
                painter.setOpacity(1.0)
            else:
                painter.setOpacity(1.0 if layerName == sel else 0.5)
            ts_path = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets", layer.layerTileset.fileName)
            tileset = QtGui.QImage(ts_path)
            if tileset.isNull():
                continue
            columns = tileset.width() // tileSize
            for y in range(self.mapData.height):
                for x in range(self.mapData.width):
                    tileNumber = layer.tiles[y][x]
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
        tiles: List[List[Optional[Engine.Gameplay.Tile]]] = []
        for y in range(height):
            row: List[Optional[Engine.Gameplay.Tile]] = []
            for x in range(width):
                row.append(None)
            tiles.append(row)
        layer = TileLayerData(name, filePath, tiles)
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
        QtWidgets.QMessageBox.information(self, "Hint", f"Chosen: ({gx}, {gy})")
