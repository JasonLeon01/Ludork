# -*- encoding: utf-8 -*-

from __future__ import annotations
import logging
import os
from typing import Any, Dict, List, Optional
from .. import (
    Vector2i,
    Vector2u,
    Color,
    Image,
    Texture,
    ReturnType,
    TileLayerGraphics,
    AutoTile,
    TileLayerData,
    Shader,
)
from ..Utils.Inner import IS_IOS_PLATFORM, warnIosShaderSkippedOnce


class TileLayer(TileLayerGraphics):
    r"""Tile layer renderable.

    Renders a tile grid using a `VertexArray` and provides
    per-cell queries for passability and material properties.
    Autotile cells are rendered with the standard RPG Maker XP 48-state
    composition through additional vertex arrays managed in the C++
    backend.
    """

    def __init__(
        self,
        data: TileLayerData,
        texture: Texture,
        autoTileTextures: Optional[List[Texture]] = None,
        autoTileFrameCounts: Optional[List[int]] = None,
        visible: bool = True,
    ) -> None:
        r"""Initialise a TileLayer.

        - \param data                TileLayerData containing the tile-index grid and autotile pool
        - \param texture             Texture (tileset image) for rendering
        - \param autoTileTextures    Per-autotile-pool textures matching `data.autoTilePool`
        - \param autoTileFrameCounts Per-autotile-pool animation frame counts
        - \param visible             Whether the layer is initially visible
        """
        from .. import CellSize

        self._data = data
        self._width = len(self._data.tiles[0]) if self._data.tiles else 0
        self._height = len(self._data.tiles)
        self._texture = texture
        self._lightBlockMapCache: Optional[List[List[float]]] = None
        self._lightBlockImageCache: Optional[Image] = None
        self._reflectionStrengthMapCache: Optional[List[List[float]]] = None
        self._reflectionStrengthImageCache: Optional[Image] = None
        self._ignoreLightingMapCache: Optional[List[List[float]]] = None
        self._ignoreLightingImageCache: Optional[Image] = None
        self.visible = visible
        self.shaderPath: str = str(self._data.shaderPath or "")
        self.shader: Optional[Shader] = None
        self.shaderType: Optional[Shader.Type] = None
        self._shaderTime: float = 0.0
        self._shaderUsesTime: bool = False
        self._loadShader()

        autoTilePool = list(self._data.autoTilePool or [])
        if not self._data.autoTileKeys:
            self._data.autoTileKeys = [entry.name for entry in autoTilePool]
        autoTileTextureList: List[Texture] = list(autoTileTextures or [])
        if len(autoTileTextureList) < len(autoTilePool):
            autoTileTextureList.extend([None] * (len(autoTilePool) - len(autoTileTextureList)))
        autoTileMaterials = [(entry.material if entry is not None else None) for entry in autoTilePool]
        autoTileFrames = list(autoTileFrameCounts or [])
        if len(autoTileFrames) < len(autoTilePool):
            autoTileFrames.extend([1] * (len(autoTilePool) - len(autoTileFrames)))
        self._autoTileTextures = autoTileTextureList
        self._autoTileFrameCounts = autoTileFrames
        self._autoTileMaterialsRef = autoTileMaterials

        super().__init__(
            self._width,
            self._height,
            CellSize,
            self._texture,
            self._data,
            autoTileTextureList,
            autoTileFrames,
        )

    def _loadShader(self) -> None:
        if not self.shaderPath:
            return
        if IS_IOS_PLATFORM:
            warnIosShaderSkippedOnce(
                f"TileLayer.shaderPath:{self.shaderPath}",
                f"iOS: shaders are disabled; skipped tile layer shader: {self.shaderPath}",
            )
            return
        extension = os.path.splitext(self.shaderPath)[1].lower()
        shaderTypes = {
            ".vert": Shader.Type.Vertex,
            ".frag": Shader.Type.Fragment,
        }
        shaderType = shaderTypes.get(extension)
        if shaderType is None:
            logging.warning("Unsupported tile layer shader extension: %s", self.shaderPath)
            return
        fullPath = self.shaderPath
        normalizedPath = fullPath.replace("\\", "/").lstrip("./")
        if not normalizedPath.lower().startswith("assets/shaders/"):
            fullPath = os.path.join(".", "Assets", "Shaders", self.shaderPath)
        if not os.path.exists(fullPath):
            logging.warning("Tile layer shader file not found: %s", fullPath)
            return
        try:
            with open(fullPath, "r", encoding="utf-8") as shaderFile:
                self._shaderUsesTime = "uniform float time" in shaderFile.read()
            self.shader = Shader(fullPath, shaderType)
            self.shaderType = shaderType
        except OSError as exc:
            logging.warning("Tile layer shader file could not be read for %s: %s", self.shaderPath, exc)
        except Exception as exc:
            logging.warning("Tile layer shader load failed for %s: %s", self.shaderPath, exc)

    def updateShader(self, deltaTime: float) -> None:
        r"""Advance the conventional time uniform for this layer shader.

        - \param deltaTime Elapsed time in seconds
        """
        if self.shader is None or not self._shaderUsesTime:
            return
        self._shaderTime += deltaTime
        self.shader.setUniform("time", self._shaderTime)

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

    @ReturnType(autoTiles=List[List[Optional[int | str]]])
    def getAutoTiles(self) -> List[List[Optional[int | str]]]:
        r"""Return the autotile pool-index grid.

        - \return  2D list of autotile pool indices (`None` = no autotile)
        """
        return self._data.autoTiles

    @ReturnType(pool=List[AutoTile])
    def getAutoTilePool(self) -> List[AutoTile]:
        r"""Return the autotile pool used by this layer.

        - \return  List of `AutoTile` entries referenced by `autoTiles`
        """
        return self._data.autoTilePool

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

    @ReturnType(ignoreLighting=bool)
    def getIgnoreLighting(self, position: Vector2i) -> bool:
        r"""Check whether the tile ignores ambient and direct lighting.

        - \param position  Grid position to query
        - \return          `True` if the surface is rendered unlit
        """
        return bool(self.getMaterialProperty(position, "ignoreLighting"))

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
        material = self.getMaterial(position)
        if material is None:
            return None
        return getattr(material, propertyName, None)

    def getLightBlockMap(self) -> List[List[float]]:
        r"""Build or return the cached light-block map.

        - \return  2D grid of light-block values (0.0–1.0)
        """
        if self._lightBlockMapCache is None:
            self._lightBlockMapCache = super().getLightBlockMap()
        return self._lightBlockMapCache

    def getReflectionStrengthMap(self) -> List[List[float]]:
        r"""Build or return the cached reflection-strength map.

        - \return  2D grid of reflection strength values
        """
        if self._reflectionStrengthMapCache is None:
            self._reflectionStrengthMapCache = super().getReflectionStrengthMap()
        return self._reflectionStrengthMapCache

    def getIgnoreLightingMap(self) -> List[List[float]]:
        r"""Build or return the cached ignore-lighting map.

        - \return  2D grid of ignore-lighting values
        """
        if self._ignoreLightingMapCache is None:
            self._ignoreLightingMapCache = super().getIgnoreLightingMap()
        return self._ignoreLightingMapCache

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

    def getIgnoreLightingImage(self) -> Image:
        r"""Build or return the cached ignore-lighting image.

        - \return  Grayscale `Image` where brightness = ignore-lighting
        """
        if self._ignoreLightingImageCache is None:
            dataMap = self.getIgnoreLightingMap()
            img = Image(Vector2u(self._width, self._height))
            for y in range(self._height):
                for x in range(self._width):
                    g = int(dataMap[y][x] * 255)
                    img.setPixel(Vector2u(x, y), Color(g, g, g))
            self._ignoreLightingImageCache = img
        return self._ignoreLightingImageCache


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

    @ReturnType(autoTiles=Dict[str, List[List[Optional[int | str]]]])
    def getAutoTilesData(self) -> Dict[str, List[List[Optional[int | str]]]]:
        r"""Return raw autotile pool-index data for every layer.

        - \return  Dictionary mapping layer name to autotile pool-index grid
        """
        return {layerName: layer.getAutoTiles() for layerName, layer in self._layers.items()}

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

    def updateAutoTileAnimation(self, deltaTime: float, frameInterval: float = 0.5) -> None:
        r"""Advance autotile animation across every layer.

        - \param deltaTime      Elapsed time in seconds
        - \param frameInterval  Seconds between two animation frames
        """
        for layer in self._layers.values():
            layer.updateAutoTileAnimation(deltaTime, frameInterval)
            layer.updateShader(deltaTime)
