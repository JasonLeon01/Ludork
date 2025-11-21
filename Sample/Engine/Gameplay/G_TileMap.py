# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass
from typing import Any, Dict, List, Optional, TYPE_CHECKING
from . import Drawable, Transformable, VertexArray, Manager, PrimitiveType, Vector2f, Vector2i, Vector2u

if TYPE_CHECKING:
    from Engine import RenderTarget, RenderStates, Vector2u


@dataclass
class Tile:
    id: int
    passible: bool = True
    lightBlock: float = 0.3


@dataclass
class TileLayerData:
    name: str
    filePath: str
    tiles: List[List[Optional[Tile]]]
    visible: bool = True


class TileLayer(TileLayerData, Drawable, Transformable):
    def __init__(
        self,
        name: str,
        filePath: str,
        tiles: List[List[Optional[Tile]]],
        visible: bool = True,
    ) -> None:
        TileLayerData.__init__(self, name, filePath, tiles, visible)
        self._width = len(tiles[0])
        self._height = len(tiles)
        self._vertexArray = VertexArray(PrimitiveType.Triangles, self._width * self._height * 6)
        self._texture = Manager.loadTileset(filePath)
        Drawable.__init__(self)
        Transformable.__init__(self)
        self._init()

    def getTiles(self) -> List[List[Optional[Tile]]]:
        return self.tiles

    def get(self, position: Vector2i) -> Optional[Tile]:
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return None
        return self.tiles[position.y][position.x]

    def isPassable(self, position: Vector2i) -> bool:
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return False
        tile = self.tiles[position.y][position.x]
        if tile is None:
            return False
        return tile.passible

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        if not self.visible:
            return

        states.transform *= self.getTransform()
        states.texture = self._texture
        target.draw(self._vertexArray, states)

    def _init(self) -> None:
        from . import GetCellSize

        tileSize = GetCellSize()
        columns = self._texture.getSize().x // tileSize

        for y in range(self._height):
            for x in range(self._width):
                tile = self.tiles[y][x]
                if tile is None:
                    continue

                tileNumber = tile.id

                tu = tileNumber % columns
                tv = tileNumber // columns
                start = (x + y * self._width) * 6

                self._vertexArray[start + 0].position = Vector2f(x * tileSize, y * tileSize)
                self._vertexArray[start + 1].position = Vector2f((x + 1) * tileSize, y * tileSize)
                self._vertexArray[start + 2].position = Vector2f(x * tileSize, (y + 1) * tileSize)
                self._vertexArray[start + 3].position = Vector2f(x * tileSize, (y + 1) * tileSize)
                self._vertexArray[start + 4].position = Vector2f((x + 1) * tileSize, y * tileSize)
                self._vertexArray[start + 5].position = Vector2f((x + 1) * tileSize, (y + 1) * tileSize)

                self._vertexArray[start + 0].texCoords = Vector2f(tu * tileSize, tv * tileSize)
                self._vertexArray[start + 1].texCoords = Vector2f((tu + 1) * tileSize, tv * tileSize)
                self._vertexArray[start + 2].texCoords = Vector2f(tu * tileSize, (tv + 1) * tileSize)
                self._vertexArray[start + 3].texCoords = Vector2f(tu * tileSize, (tv + 1) * tileSize)
                self._vertexArray[start + 4].texCoords = Vector2f((tu + 1) * tileSize, tv * tileSize)
                self._vertexArray[start + 5].texCoords = Vector2f((tu + 1) * tileSize, (tv + 1) * tileSize)


class Tilemap:
    def __init__(self, layers: List[TileLayer]) -> None:
        self._layers: Dict[str, TileLayer] = {}
        for layer in layers:
            self._layers[layer.name] = layer

    def getLayer(self, name: str) -> Optional[TileLayer]:
        for layerName, layer in self._layers.items():
            if layerName == name:
                return layer
        return None

    def getAllLayers(self) -> Dict[str, TileLayer]:
        return self._layers

    def getLayerNameList(self) -> List[TileLayer]:
        return list(self._layers.keys())

    def getSize(self) -> Vector2u:
        if len(self._layers) == 0:
            return Vector2u(0, 0)
        first = next(iter(self._layers.values()))
        return Vector2u(first._width, first._height)

    @staticmethod
    def loadData(data: Dict[str, List[List[Any]]], width: int, height: int) -> Tilemap:
        mapLayers = []
        for layerName, layerData in data.items():
            layerTiles = layerData["tiles"]
            tiles: List[List[Tile]] = []
            for y in range(height):
                tiles.append([])
                for x in range(width):
                    tileInfo = layerTiles[y][x]
                    if tileInfo is None:
                        tiles[-1].append(None)
                    else:
                        tiles[-1].append(Tile(*tileInfo))
            layer = TileLayer(layerName, layerData["filePath"], tiles)
            mapLayers.append(layer)
        return Tilemap(mapLayers)
