# -*- encoding: utf-8 -*-

from __future__ import annotations
from dataclasses import dataclass, asdict
from typing import Any, Dict, List, Optional
from .. import (
    Tuple4,
    VertexArray,
    PrimitiveType,
    Vector2i,
    Vector2u,
    Color,
    Image,
    Texture,
    ReturnType,
    TileLayerGraphics,
)
from ..Utils import Inner
from .Material import Material


@dataclass
class Tileset:
    r"""Tileset dataclass.

    Defines a tileset image and per-tile properties.
    """

    name: str  #: Tileset name
    fileName: str  #: Texture image file name
    passable: List[bool]  #: Per-tile passability (True = can walk)
    materials: List[Material]  #: Per-tile material references
    dir4: List[Tuple4[bool]]  #: Per-tile 4-direction passability

    def asDict(self) -> Dict[str, Any]:
        r"""Serialize the tileset to a dictionary.

        - \return  Dictionary containing all tileset fields
        """
        return asdict(self)

    @staticmethod
    def fromData(data: Dict[str, Any]) -> Tileset:
        r"""Create a Tileset from a raw data dictionary.

        - \param data  Raw dictionary, e.g. loaded from JSON
        - \return      The created `Tileset` instance
        """
        if isinstance(data["materials"][0], dict):
            data["materials"] = [
                Material(**Inner.filterDataClassParams(materialData, Material)) for materialData in data["materials"]
            ]
        return Tileset(**data)


@dataclass
class TileLayerData:
    r"""Tile layer data dataclass.

    Stores the raw tile-index grid for one layer.
    """

    layerName: str  #: Name of the layer
    layerTileset: Tileset  #: Tileset used by this layer
    tiles: List[List[Optional[int]]]  #: 2D grid of tile indices (None = empty)


class TileLayer(TileLayerGraphics):
    r"""Tile layer renderable.

    Renders a tile grid using a `VertexArray` and provides
    per-cell queries for passability and material properties.
    """

    def __init__(
        self,
        data: TileLayerData,
        texture: Texture,
        visible: bool = True,
    ) -> None:
        r"""Initialise a TileLayer.

        - \param data      TileLayerData containing the tile-index grid
        - \param texture  Texture (tileset image) for rendering
        - \param visible  Whether the layer is initially visible
        """
        from .. import CellSize

        self._data = data
        self._width = len(self._data.tiles[0])
        self._height = len(self._data.tiles)
        self._vertexArray = VertexArray(PrimitiveType.Triangles, self._width * self._height * 6)
        self._texture = texture
        self._lightBlockMapCache: Optional[List[List[float]]] = None
        self._lightBlockImageCache: Optional[Image] = None
        self._reflectionStrengthMapCache: Optional[List[List[float]]] = None
        self._reflectionStrengthImageCache: Optional[Image] = None
        self.visible = visible
        super().__init__(
            self._width, self._height, CellSize, self._texture, self._data.tiles, self._data.layerTileset.materials
        )

    @ReturnType(name=str)
    def getName(self) -> str:
        r"""Return the layer name.

        - \return  The `layerName` string
        """
        return self._data.layerName

    @ReturnType(tiles=List[List[Optional[int]]])
    def getTiles(self) -> List[List[Optional[int]]]:
        r"""Return the tile-index grid.

        - \return  2D list of tile indices (`None` = empty cell)
        """
        return self._data.tiles

    @ReturnType(tile=Optional[int])
    def get(self, position: Vector2i) -> Optional[int]:
        r"""Return the tile index at the given position.

        - \param position  Grid position to query
        - \return          Tile index, or `None` if out of bounds/empty
        """
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return None
        return self._data.tiles[position.y][position.x]

    @ReturnType(isPassable=bool)
    def isPassable(self, position: Vector2i) -> bool:
        r"""Check whether a cell is passable.

        - \param position  Grid position to query
        - \return        `True` if the cell can be walked on
        """
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return False
        tileNumber = self._data.tiles[position.y][position.x]
        if tileNumber is None:
            return True
        return self._data.layerTileset.passable[tileNumber]

    @ReturnType(material=Optional[Material])
    def getMaterial(self, position: Vector2i) -> Optional[Material]:
        r"""Return the material at the given position.

        - \param position  Grid position to query
        - \return          `Material` instance, or `None`
        """
        if position.x < 0 or position.y < 0 or position.x >= self._width or position.y >= self._height:
            return None
        tileNumber = self._data.tiles[position.y][position.x]
        if tileNumber is None:
            return None
        return self._data.layerTileset.materials[tileNumber]

    @ReturnType(lightBlock=float)
    def getLightBlock(self, position: Vector2i) -> float:
        r"""Return the light-block value at the given position.

        - \param position  Grid position to query
        - \return          Light-block factor (0.0–1.0)
        """
        return self.getMaterialProperty(position, "lightBlock")

    @ReturnType(mirror=bool)
    def getMirror(self, position: Vector2i) -> bool:
        r"""Check whether the tile mirrors at the given position.

        - \param position  Grid position to query
        - \return          `True` if the surface mirrors
        """
        return self.getMaterialProperty(position, "mirror")

    @ReturnType(reflectionStrength=float)
    def getReflectionStrength(self, position: Vector2i) -> float:
        r"""Return the reflection strength at the given position.

        - \param position  Grid position to query
        - \return          Reflection strength (0.0–1.0)
        """
        return self.getMaterialProperty(position, "reflectionStrength")

    @ReturnType(emissive=float)
    def getEmissive(self, position: Vector2i) -> float:
        r"""Return the emissive value at the given position.

        - \param position  Grid position to query
        - \return          Emissive lighting factor
        """
        return self.getMaterialProperty(position, "emissive")

    @ReturnType(speedRate=Optional[float])
    def getSpeedRate(self, position: Vector2i) -> Optional[float]:
        r"""Return the movement speed rate at the given position.

        - \param position  Grid position to query
        - \return          Speed multiplier, or `None`
        """
        return self.getMaterialProperty(position, "speedRate")

    def getMaterialProperty(self, position: Vector2i, propertyName: str) -> Any:
        r"""Return an arbitrary material property at the given position.

        - \param position      Grid position to query
        - \param propertyName  Name of the material attribute to read
        - \return              Property value, or `None` if no material
        """
        result = self.getMaterial(position)
        if result is None:
            return None
        return getattr(result, propertyName)

    def getLightBlockMap(self) -> List[List[float]]:
        r"""Build or return the cached light-block map.

        - \return  2D grid of light-block values (0.0–1.0)
        """
        if self._lightBlockMapCache is None:
            self._lightBlockMapCache = [
                [(self.getLightBlock(Vector2i(x, y)) or 0.0) for x in range(self._width)] for y in range(self._height)
            ]
        return self._lightBlockMapCache

    def getReflectionStrengthMap(self) -> List[List[float]]:
        r"""Build or return the cached reflection-strength map.

        - \return  2D grid of reflection strength values
        """
        if self._reflectionStrengthMapCache is None:
            self._reflectionStrengthMapCache = [
                [
                    (self.getMirror(Vector2i(x, y)) and self.getReflectionStrength(Vector2i(x, y)) or 0.0)
                    for x in range(self._width)
                ]
                for y in range(self._height)
            ]
        return self._reflectionStrengthMapCache

    def getLightBlockImage(self) -> Image:
        r"""Build or return the cached light-block image.

        - \return  Grayscale `Image` where brightness = light block
        """
        if self._lightBlockImageCache is None:
            dataMap = self.getLightBlockMap()
            img = Image(Vector2u(self._width, self._height))
            for y in range(self._height):
                for x in range(self._width):
                    g = int(dataMap[y][x] * 255)
                    img.setPixel(Vector2u(x, y), Color(g, g, g))
            self._lightBlockImageCache = img
        return self._lightBlockImageCache

    def getReflectionStrengthImage(self) -> Image:
        r"""Build or return the cached reflection-strength image.

        - \return  Grayscale `Image` where brightness = reflection strength
        """
        if self._reflectionStrengthImageCache is None:
            dataMap = self.getReflectionStrengthMap()
            img = Image(Vector2u(self._width, self._height))
            for y in range(self._height):
                for x in range(self._width):
                    g = int(dataMap[y][x] * 255)
                    img.setPixel(Vector2u(x, y), Color(g, g, g))
            self._reflectionStrengthImageCache = img
        return self._reflectionStrengthImageCache


class Tilemap:
    r"""Tilemap container.

    Holds multiple named `TileLayer`s and provides
    aggregated queries across all layers.
    """

    def __init__(self, layers: List[TileLayer]) -> None:
        r"""Initialise a Tilemap from a list of layers.

        - \param layers  List of `TileLayer` instances to register
        """
        self._layers: Dict[str, TileLayer] = {}
        for layer in layers:
            self._layers[layer.getName()] = layer
        self._tilesData: Dict[str, List[List[Optional[int]]]] = {}
        for layerName, layer in self._layers.items():
            self._tilesData[layerName] = layer.getTiles()

    @ReturnType(layer=Optional[TileLayer])
    def getLayer(self, name: str) -> Optional[TileLayer]:
        r"""Return a layer by name.

        - \param name   Name of the layer to retrieve
        - \return         The `TileLayer`, or `None`
        """
        for layerName, layer in self._layers.items():
            if layerName == name:
                return layer
        return None

    @ReturnType(tiles=Dict[str, List[List[Optional[int]]]])
    def getTilesData(self) -> Dict[str, List[List[Optional[int]]]]:
        r"""Return raw tile-index data for all layers.

        - \return  Dictionary mapping layer name to tile-index grid
        """
        return self._tilesData

    @ReturnType(layers=Dict[str, TileLayer])
    def getAllLayers(self) -> Dict[str, TileLayer]:
        r"""Return all layers.

        - \return  Dictionary mapping layer name to `TileLayer`
        """
        return self._layers

    @ReturnType(layerNames=List[str])
    def getLayerNameList(self) -> List[str]:
        r"""Return all layer names.

        - \return  List of layer name strings
        """
        return list(self._layers.keys())

    @ReturnType(size=Vector2u)
    def getSize(self) -> Vector2u:
        r"""Return the map dimensions in tiles.

        - \return  `(width, height)` as `Vector2u`
        """
        if len(self._layers) == 0:
            return Vector2u(0, 0)
        first = next(iter(self._layers.values()))
        return Vector2u(first._width, first._height)
