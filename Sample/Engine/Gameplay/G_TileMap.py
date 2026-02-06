# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass, asdict
from typing import Any, Dict, List, Optional
from .. import (
    VertexArray,
    Manager,
    PrimitiveType,
    Vector2i,
    Vector2u,
    GetCellSize,
    Color,
    Image,
    Drawable,
    Transformable,
)
from .G_Material import Material

try:
    from ..GraphicsExtension import TileLayerGraphics
except ImportError:

    class TileLayerGraphics(Drawable, Transformable):
        pass


@dataclass
class Tileset:
    name: str
    fileName: str
    passable: List[bool]
    materials: List[Material]

    def asDict(self) -> Dict[str, Any]:
        return asdict(self)

    @staticmethod
    def fromData(data: Dict[str, Any]):
        if isinstance(data["materials"][0], dict):
            data["materials"] = [Material(**materialData) for materialData in data["materials"]]
        return Tileset(**data)


@dataclass
class TileLayerData:
    layerName: str
    layerTileset: Tileset
    tiles: List[List[Optional[int]]]


class TileLayer(TileLayerGraphics):
    def __init__(
        self,
        data: TileLayerData,
        visible: bool = True,
    ) -> None:
        self._data = data
        self._width = len(self._data.tiles[0])
        self._height = len(self._data.tiles)
        self._vertexArray = VertexArray(PrimitiveType.Triangles, self._width * self._height * 6)
        self._texture = Manager.loadTileset(self._data.layerTileset.fileName)
        self._lightBlockMapCache: Optional[List[List[float]]] = None
        self._lightBlockImageCache: Optional[Image] = None
        self.visible = visible
        super().__init__(
            self._width, self._height, GetCellSize(), self._texture, self._data.tiles, self._data.layerTileset.materials
        )

    def getName(self) -> str:
        return self._data.layerName

    def getTiles(self) -> List[List[Optional[int]]]:
        return self._data.tiles

    def get(self, position: Vector2i) -> Optional[int]:
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return None
        return self._data.tiles[position.y][position.x]

    def isPassable(self, position: Vector2i) -> bool:
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return False
        tileNumber = self._data.tiles[position.y][position.x]
        if tileNumber is None:
            return True
        return self._data.layerTileset.passable[tileNumber]

    def getMaterial(self, position: Vector2i) -> Any:
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return None
        tileNumber = self._data.tiles[position.y][position.x]
        if tileNumber is None:
            return None
        return self._data.layerTileset.materials[tileNumber]

    def getMaterialProperty(self, position: Vector2i, propertyName: str) -> Any:
        result = self.getMaterial(position)
        if result is None:
            return None
        return getattr(result, propertyName)

    def getLightBlockMap(self) -> List[List[float]]:
        if self._lightBlockMapCache is None:
            self._lightBlockMapCache = [
                [(self.getLightBlock(Vector2i(x, y)) or 0.0) for x in range(self._width)] for y in range(self._height)
            ]
        return self._lightBlockMapCache

    def getLightBlockImage(self) -> Image:
        if self._lightBlockImageCache is None:
            dataMap = self.getLightBlockMap()
            img = Image(Vector2u(self._width, self._height))
            for y in range(self._height):
                for x in range(self._width):
                    g = int(dataMap[y][x] * 255)
                    img.setPixel(Vector2u(x, y), Color(g, g, g))
            self._lightBlockImageCache = img
        return self._lightBlockImageCache

    def getLightBlock(self, position: Vector2i) -> float:
        return self.getMaterialProperty(position, "lightBlock")

    def getMirror(self, position: Vector2i) -> bool:
        return self.getMaterialProperty(position, "mirror")

    def getReflectionStrength(self, position: Vector2i) -> float:
        return self.getMaterialProperty(position, "reflectionStrength")

    def getEmissive(self, position: Vector2i) -> float:
        return self.getMaterialProperty(position, "emissive")

    def getSpeedRate(self, position: Vector2i) -> Optional[float]:
        return self.getMaterialProperty(position, "speedRate")


class Tilemap:
    def __init__(self, layers: List[TileLayer]) -> None:
        self._layers: Dict[str, TileLayer] = {}
        for layer in layers:
            self._layers[layer.getName()] = layer
        self._tilesData: Dict[str, List[List[Optional[int]]]] = {}
        for layerName, layer in self._layers.items():
            self._tilesData[layerName] = layer.getTiles()

    def getLayer(self, name: str) -> Optional[TileLayer]:
        for layerName, layer in self._layers.items():
            if layerName == name:
                return layer
        return None

    def getTilesData(self) -> Dict[str, List[List[Optional[int]]]]:
        return self._tilesData

    def getAllLayers(self) -> Dict[str, TileLayer]:
        return self._layers

    def getLayerNameList(self) -> List[str]:
        return list(self._layers.keys())

    def getSize(self) -> Vector2u:
        if len(self._layers) == 0:
            return Vector2u(0, 0)
        first = next(iter(self._layers.values()))
        return Vector2u(first._width, first._height)

    @staticmethod
    def fromData(data: Dict[str, List[List[Any]]], width: int, height: int) -> Tilemap:
        from Source import Data

        mapLayers = []
        for layerName, layerData in data.items():
            name = layerData["layerName"]
            layerTileset = Data.getTileset(layerData["layerTileset"])
            layerTiles = layerData["tiles"]
            tiles: List[List[Optional[int]]] = []
            for y in range(height):
                tiles.append([])
                for x in range(width):
                    tileNumber = layerTiles[y][x]
                    tiles[-1].append(tileNumber)
            layer = TileLayer(
                TileLayerData(
                    name,
                    layerTileset,
                    tiles,
                ),
            )
            mapLayers.append(layer)
        return Tilemap(mapLayers)
