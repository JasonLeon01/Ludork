# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass, field
from typing import Dict, List, Optional, TYPE_CHECKING
from . import Drawable, Transformable, VertexArray, Manager, PrimitiveType, Vector2f

if TYPE_CHECKING:
    from Engine import Texture, RenderTarget, RenderStates


class TileLayer(Drawable, Transformable):
    def __init__(
        self,
        name: str = "",
        filePath: str = "",
        tiles: List[List[int]] = field(default_factory=list),
        visible: bool = True,
    ) -> None:
        self.name = name
        self.visible = visible
        self._filePath = filePath
        self._tiles = tiles
        self._width = len(tiles[0])
        self._height = len(tiles)
        self._vertexArray = VertexArray(PrimitiveType.Triangles, self._width * self._height * 6)
        self._texture = Manager.loadTileset(filePath)
        Drawable.__init__(self)
        Transformable.__init__(self)
        self._init()

    def getTiles(self) -> List[List[int]]:
        return self._tiles

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        states.transform *= self.getTransform()
        texture = states.texture

        if not self.visible:
            return

        states.texture = self._texture
        target.draw(self._vertexArray, states)

        states.texture = texture
        states.transform *= self.getTransform().getInverse()

    def _init(self) -> None:
        from Engine import GetCellSize

        tileSize = GetCellSize()
        columns = self._texture.getSize().x // tileSize

        for y in range(self._height):
            for x in range(self._width):
                tile = self._tiles[y][x]
                if tile is None:
                    continue

                tu = tile % columns
                tv = tile // columns
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
        for layer in self._layers:
            if layer.name == name:
                return layer
        return None

    def getAllLayers(self) -> Dict[str, TileLayer]:
        return self._layers

    def getLayerNameList(self) -> List[TileLayer]:
        return list(self._layers.keys())
