# -*- encoding: utf-8 -*-

import os
from typing import Dict, List, Optional, Tuple
from PyQt5 import QtCore, QtGui
from EditorGlobal import EditorStatus, GameData
from .MapRenderUtils import GridToStringGrid, QImageToRgbaTuple, RgbaBytesToQImage

_CELL = 32


def _frameCount(image: QtGui.QImage, cellSize: int) -> int:
    if image.isNull():
        return 1
    w = image.width()
    return max(1, w // (3 * cellSize))


class AutoTileRenderer:

    def __init__(self) -> None:
        self._sourceImages: Dict[str, QtGui.QImage] = {}
        self._frameCounts: Dict[str, int] = {}
        self._tileCache: Dict[Tuple[str, int, int], QtGui.QImage] = {}

    def invalidate(self) -> None:
        self._sourceImages.clear()
        self._frameCounts.clear()
        self._tileCache.clear()

    def invalidateKey(self, key: str) -> None:
        self._sourceImages.pop(key, None)
        self._frameCounts.pop(key, None)
        keysToDrop = [k for k in self._tileCache.keys() if k[0] == key]
        for k in keysToDrop:
            del self._tileCache[k]

    def frameCountFor(self, key: str) -> int:
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
        return self._loadSource(key)

    def renderTile(self, key: str, mask: int, frame: int, tileSize: int = _CELL) -> Optional[QtGui.QImage]:
        img = self._loadSource(key)
        if img is None:
            return None
        frames = self._frameCounts.get(key, 1)
        frameMod = frame % frames if frames > 0 else 0
        normalized = NormalizeMask(mask)
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

        rgba, w, h, stride = QImageToRgbaTuple(source)
        data = C_ComposeAutoTileRGBA(rgba, w, h, stride, mask, frame, _CELL)
        return RgbaBytesToQImage(bytes(data), _CELL, _CELL)


def NormalizeMask(mask: int) -> int:
    from EditorExtensions.EditorExt import C_NormalizeAutoTileMask

    return int(C_NormalizeAutoTileMask(mask))


def ComputeMaskFromGrid(autoTiles: List[List[Optional[str]]], x: int, y: int) -> int:
    from EditorExtensions.EditorExt import C_ComputeAutoTileMaskFromGrid

    return int(C_ComputeAutoTileMaskFromGrid(GridToStringGrid(autoTiles), x, y))
