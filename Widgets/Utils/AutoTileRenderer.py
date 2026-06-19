# -*- encoding: utf-8 -*-

import os
from typing import Dict, List, Optional, Tuple
from PyQt5 import QtCore, QtGui
from EditorGlobal import EditorStatus, GameData
from .MapRenderUtils import gridToStringGrid, qimageToRgbaTuple, rgbaBytesToQImage

_CELL = 32


def _frameCount(image: QtGui.QImage, cellSize: int) -> int:
    if image.isNull():
        return 1
    w = image.width()
    return max(1, w // (3 * cellSize))


class AutoTileRenderer:
    r"""Renders RPG Maker XP-style autotile tiles to QImage tiles.

    Implements the standard 48-state autotile composition by selecting one
    16x16 source quadrant per 32x32 output tile, using both 4-direction and
    diagonal neighbour information. Caches per `(key, mask, frame)` and per
    loaded source image.
    """

    def __init__(self) -> None:
        self._sourceImages: Dict[str, QtGui.QImage] = {}
        self._frameCounts: Dict[str, int] = {}
        self._tileCache: Dict[Tuple[str, int, int], QtGui.QImage] = {}

    def invalidate(self) -> None:
        r"""Drop all cached source images and rendered tiles."""
        self._sourceImages.clear()
        self._frameCounts.clear()
        self._tileCache.clear()

    def invalidateKey(self, key: str) -> None:
        r"""Drop caches for a specific autotile key.

        - \param key  The autotile key whose caches should be cleared.
        """
        self._sourceImages.pop(key, None)
        self._frameCounts.pop(key, None)
        keysToDrop = [k for k in self._tileCache.keys() if k[0] == key]
        for k in keysToDrop:
            del self._tileCache[k]

    def frameCountFor(self, key: str) -> int:
        r"""Return the number of animation frames in the autotile image.

        - \param key  Autotile key.
        - \return     Frame count (>=1).
        """
        if key in self._frameCounts:
            return self._frameCounts[key]
        img = self._loadSource(key)
        if img is None:
            self._frameCounts[key] = 1
            return 1
        n = _frameCount(img, _CELL)
        self._frameCounts[key] = n
        return n

    def getSourceImage(self, key: str) -> Optional[QtGui.QImage]:
        r"""Return the loaded source image for an autotile key.

        - \param key  Autotile key.
        - \return     Source `QImage`, or `None` when missing.
        """
        return self._loadSource(key)

    def renderTile(self, key: str, mask: int, frame: int, tileSize: int = _CELL) -> Optional[QtGui.QImage]:
        r"""Render one autotile composite tile.

        - \param key      Autotile key.
        - \param mask     8-bit neighbour mask: bit0 top, bit1 right, bit2 bottom,
                          bit3 left, bit4 top-left, bit5 top-right, bit6 bottom-right,
                          bit7 bottom-left.
        - \param frame    Animation frame index (will be wrapped by frame count).
        - \param tileSize Output tile size in pixels.
        - \return         The composed `QImage`, or `None` when the source is missing.
        """
        img = self._loadSource(key)
        if img is None:
            return None
        frames = self._frameCounts.get(key, 1)
        frameMod = frame % frames if frames > 0 else 0
        normalized = normalizeMask(mask)
        cacheKey = (key, normalized, frameMod)
        cached = self._tileCache.get(cacheKey)
        if cached is not None:
            if tileSize == _CELL:
                return cached
            return cached.scaled(tileSize, tileSize, QtCore.Qt.IgnoreAspectRatio, QtCore.Qt.SmoothTransformation)
        result = self._composeTile(img, normalized, frameMod)
        self._tileCache[cacheKey] = result
        if tileSize == _CELL:
            return result
        return result.scaled(tileSize, tileSize, QtCore.Qt.IgnoreAspectRatio, QtCore.Qt.SmoothTransformation)

    def _loadSource(self, key: str) -> Optional[QtGui.QImage]:
        if key in self._sourceImages:
            return self._sourceImages[key]
        data = GameData.autoTileData.get(key)
        if data is None:
            return None
        fileName = getattr(data, "fileName", "") or ""
        if not fileName:
            return None
        path = os.path.join(EditorStatus.PROJ_PATH, "Assets", "Autotiles", fileName)
        if not os.path.exists(path):
            return None
        img = QtGui.QImage(path)
        if img.isNull():
            return None
        self._sourceImages[key] = img
        self._frameCounts[key] = _frameCount(img, _CELL)
        return img

    def _composeTile(self, source: QtGui.QImage, mask: int, frame: int) -> QtGui.QImage:
        from EditorExtensions.EditorExt import C_ComposeAutoTileRGBA

        rgba, w, h, stride = qimageToRgbaTuple(source)
        data = C_ComposeAutoTileRGBA(rgba, w, h, stride, mask, frame, _CELL)
        return rgbaBytesToQImage(bytes(data), _CELL, _CELL)


def normalizeMask(mask: int) -> int:
    r"""Normalise an 8-bit autotile neighbour mask.

    - \param mask  Raw 8-bit mask.
    - \return      Normalised mask suitable as a cache key.
    """
    from EditorExtensions.EditorExt import C_NormalizeAutoTileMask

    return int(C_NormalizeAutoTileMask(mask))


def computeMaskFromGrid(autoTiles: List[List[Optional[str]]], x: int, y: int) -> int:
    r"""Compute the 8-direction connectivity mask for an autotile cell.

    The mask packs neighbour matches as bit0=top, bit1=right, bit2=bottom,
    bit3=left, bit4=top-left, bit5=top-right, bit6=bottom-right,
    bit7=bottom-left. Cells outside the grid are treated as non-matching.

    - \param autoTiles  2D grid of autotile keys.
    - \param x          Grid x coordinate.
    - \param y          Grid y coordinate.
    - \return           8-bit neighbour mask (0..255).
    """
    from EditorExtensions.EditorExt import C_ComputeAutoTileMaskFromGrid

    return int(C_ComputeAutoTileMaskFromGrid(gridToStringGrid(autoTiles), x, y))
