# -*- encoding: utf-8 -*-

import array
import os
from typing import Any, Dict, List, Optional, Tuple
from PyQt5 import QtGui
from EditorGlobal import EditorStatus, GameData
from .AutoTileRenderer import AutoTileRenderer
from .MapRenderUtils import gridToStringGrid, qimageToRgbaTuple, rgbaBytesToQImage

_SOURCE_TILE_SIZE = 32


def buildTilesBuffer(tiles: Any, mapW: int, mapH: int) -> array.array:
    buf = array.array("i", [-1] * (mapW * mapH))
    if not isinstance(tiles, list):
        return buf
    for y in range(min(mapH, len(tiles))):
        row = tiles[y]
        if not isinstance(row, list):
            continue
        for x in range(min(mapW, len(row))):
            tileNumber = row[x]
            if tileNumber is None:
                continue
            try:
                buf[y * mapW + x] = int(tileNumber)
            except (TypeError, ValueError):
                continue
    return buf


class TilemapRenderer:
    r"""Renders editor map layers (tile indices and autotiles) via EditorExt."""

    def __init__(self, autoTileRenderer: AutoTileRenderer) -> None:
        self._autoTileRenderer = autoTileRenderer
        self._tilesetImages: Dict[str, QtGui.QImage] = {}

    def invalidate(self) -> None:
        r"""Drop all cached tileset images."""
        self._tilesetImages.clear()

    def invalidateTileset(self, key: str) -> None:
        r"""Drop the cached tileset image for a tileset key.

        - \param key  Tileset data key.
        """
        self._tilesetImages.pop(key, None)

    def renderLayer(
        self,
        mapW: int,
        mapH: int,
        outputTileSize: int,
        tiles: Any,
        tilesetKey: Any,
        autoTiles: Any,
        autoTileFrame: int = 0,
        sourceTileSize: int = _SOURCE_TILE_SIZE,
    ) -> Optional[QtGui.QImage]:
        r"""Render one map layer (tiles + autotiles) into a single image.

        - \param mapW            Layer width in tiles.
        - \param mapH            Layer height in tiles.
        - \param outputTileSize  Display tile size in pixels.
        - \param tiles           2D tile index grid.
        - \param tilesetKey      Tileset data key or object with fileName.
        - \param autoTiles       2D autotile key grid.
        - \param autoTileFrame   Autotile animation frame index.
        - \param sourceTileSize  Tile size in source assets.
        - \return                Composed layer image, or ``None`` when empty.
        """
        from EditorExtensions.EditorExt import C_RenderMapLayerRGBA

        tilesetTuple: Optional[Tuple[bytes, int, int, int]] = None
        tilesBuffer: Optional[array.array] = None
        tilesetImage = self._loadTileset(tilesetKey)
        if tilesetImage is not None:
            tilesetTuple = qimageToRgbaTuple(tilesetImage)
            tilesBuffer = buildTilesBuffer(tiles, mapW, mapH)

        stringGrid = gridToStringGrid(autoTiles) if isinstance(autoTiles, list) else []
        autoTileSources = self._collectAutoTileSources(stringGrid)
        hasTiles = tilesetTuple is not None and tilesBuffer is not None
        hasAutoTiles = bool(autoTileSources)
        if not hasTiles and not hasAutoTiles:
            return None

        data = C_RenderMapLayerRGBA(
            mapW,
            mapH,
            sourceTileSize,
            outputTileSize,
            tilesetTuple,
            tilesBuffer,
            autoTileFrame,
            stringGrid if hasAutoTiles else None,
            autoTileSources if hasAutoTiles else None,
        )
        return rgbaBytesToQImage(bytes(data), mapW * outputTileSize, mapH * outputTileSize)

    def _loadTileset(self, tilesetKey: Any) -> Optional[QtGui.QImage]:
        data = None
        if isinstance(tilesetKey, str):
            data = GameData.tilesetData.get(tilesetKey)
        elif tilesetKey is not None:
            data = tilesetKey
        if data is None:
            return None
        cacheKey = getattr(data, "key", None) or getattr(data, "fileName", None) or str(tilesetKey)
        if cacheKey in self._tilesetImages:
            return self._tilesetImages[cacheKey]
        fileName = getattr(data, "fileName", "") or ""
        if not fileName:
            return None
        path = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Tilesets", fileName)
        if not os.path.exists(path):
            return None
        image = QtGui.QImage(path)
        if image.isNull():
            return None
        self._tilesetImages[str(cacheKey)] = image
        return image

    def _collectAutoTileSources(
        self, stringGrid: List[List[str]]
    ) -> Dict[str, Tuple[bytes, int, int, int]]:
        keysNeeded: set[str] = set()
        for row in stringGrid:
            for key in row:
                if key:
                    keysNeeded.add(key)
        sources: Dict[str, Tuple[bytes, int, int, int]] = {}
        for key in keysNeeded:
            image = self._autoTileRenderer.getSourceImage(key)
            if image is None:
                continue
            sources[key] = qimageToRgbaTuple(image)
        return sources
