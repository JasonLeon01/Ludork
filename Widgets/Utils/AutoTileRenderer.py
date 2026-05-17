# -*- encoding: utf-8 -*-

import os
from typing import Dict, List, Optional, Tuple
from PyQt5 import QtCore, QtGui
from EditorGlobal import EditorStatus, GameData


_CELL = 32
_HALF = _CELL // 2

MASK_TOP = 0x01
MASK_RIGHT = 0x02
MASK_BOTTOM = 0x04
MASK_LEFT = 0x08
MASK_TOP_LEFT = 0x10
MASK_TOP_RIGHT = 0x20
MASK_BOTTOM_RIGHT = 0x40
MASK_BOTTOM_LEFT = 0x80

_QUADRANT_OFFSETS = (
    (0, 0),
    (1, 0),
    (0, 1),
    (1, 1),
)

_QUAD_BITS = (
    (MASK_TOP, MASK_LEFT, MASK_TOP_LEFT),
    (MASK_TOP, MASK_RIGHT, MASK_TOP_RIGHT),
    (MASK_BOTTOM, MASK_LEFT, MASK_BOTTOM_LEFT),
    (MASK_BOTTOM, MASK_RIGHT, MASK_BOTTOM_RIGHT),
)

_INNER_FILLER_CELL = 3

_BASE_PATTERN: Dict[int, Tuple[int, int, int, int]] = {
    0x00: (1, 1, 1, 1),
    0x01: (10, 12, 10, 12),
    0x02: (4, 4, 10, 10),
    0x03: (10, 10, 10, 10),
    0x04: (4, 6, 4, 6),
    0x05: (7, 9, 7, 9),
    0x06: (4, 4, 4, 4),
    0x07: (7, 7, 7, 7),
    0x08: (6, 6, 12, 12),
    0x09: (12, 12, 12, 12),
    0x0A: (5, 5, 11, 11),
    0x0B: (11, 11, 11, 11),
    0x0C: (6, 6, 6, 6),
    0x0D: (9, 9, 9, 9),
    0x0E: (5, 5, 5, 5),
    0x0F: (8, 8, 8, 8),
}


def _cellTopLeftInArea(cellIndex0Based: int) -> Tuple[int, int]:
    col = cellIndex0Based % 3
    row = cellIndex0Based // 3
    return col * _CELL, row * _CELL


def _composeCellPattern(mask: int) -> Tuple[int, int, int, int]:
    r"""Return the 4 source cell indices (1-based) for an 8-bit neighbour mask.

    Uses the canonical RPG Maker XP autotile composition: a base 4-quadrant
    pattern is selected by the orthogonal neighbour mask, then quadrants whose
    two adjacent orthos are both connected but whose diagonal is missing are
    patched with the inner-corner filler (cell 3).

    - \param mask  8-bit neighbour mask (bit0=top, bit1=right, bit2=bottom,
                   bit3=left, bit4=top-left, bit5=top-right, bit6=bottom-right,
                   bit7=bottom-left).
    - \return      4-tuple of 1-based source cell indices (TL, TR, BL, BR).
    """
    orthoMask = mask & 0x0F
    base = _BASE_PATTERN[orthoMask]
    out = list(base)
    for quadrant in range(4):
        oa, ob, d = _QUAD_BITS[quadrant]
        if (mask & oa) and (mask & ob) and not (mask & d):
            out[quadrant] = _INNER_FILLER_CELL
    return out[0], out[1], out[2], out[3]


def _normalizeMask(mask: int) -> int:
    r"""Strip irrelevant diagonal bits from a neighbour mask.

    Diagonal neighbours only affect rendering when both of their adjacent
    orthogonal neighbours are connected; clearing irrelevant diagonals keeps
    the cache key compact and well-defined.

    - \param mask  Raw 8-bit mask.
    - \return      Normalised 8-bit mask suitable for use as a cache key.
    """
    result = mask & 0x0F
    if (mask & MASK_TOP) and (mask & MASK_LEFT) and (mask & MASK_TOP_LEFT):
        result |= MASK_TOP_LEFT
    if (mask & MASK_TOP) and (mask & MASK_RIGHT) and (mask & MASK_TOP_RIGHT):
        result |= MASK_TOP_RIGHT
    if (mask & MASK_BOTTOM) and (mask & MASK_RIGHT) and (mask & MASK_BOTTOM_RIGHT):
        result |= MASK_BOTTOM_RIGHT
    if (mask & MASK_BOTTOM) and (mask & MASK_LEFT) and (mask & MASK_BOTTOM_LEFT):
        result |= MASK_BOTTOM_LEFT
    return result


def _frameCount(image: QtGui.QImage) -> int:
    if image.isNull():
        return 1
    w = image.width()
    return max(1, w // (3 * _CELL))


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
        n = _frameCount(img)
        self._frameCounts[key] = n
        return n

    def renderTile(self, key: str, mask: int, frame: int) -> Optional[QtGui.QImage]:
        r"""Render one 32x32 autotile composite tile.

        - \param key    Autotile key.
        - \param mask   8-bit neighbour mask: bit0 top, bit1 right, bit2 bottom,
                        bit3 left, bit4 top-left, bit5 top-right, bit6 bottom-right,
                        bit7 bottom-left.
        - \param frame  Animation frame index (will be wrapped by frame count).
        - \return       The composed `QImage`, or `None` when the source is missing.
        """
        img = self._loadSource(key)
        if img is None:
            return None
        frames = self._frameCounts.get(key, 1)
        frameMod = frame % frames if frames > 0 else 0
        normalized = _normalizeMask(mask)
        cacheKey = (key, normalized, frameMod)
        cached = self._tileCache.get(cacheKey)
        if cached is not None:
            return cached
        result = self._composeTile(img, normalized, frameMod)
        self._tileCache[cacheKey] = result
        return result

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
        self._frameCounts[key] = _frameCount(img)
        return img

    def _composeTile(self, source: QtGui.QImage, mask: int, frame: int) -> QtGui.QImage:
        out = QtGui.QImage(_CELL, _CELL, QtGui.QImage.Format_ARGB32_Premultiplied)
        out.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(out)
        frameOffsetX = frame * 3 * _CELL
        cells = _composeCellPattern(mask)
        for quadrant in range(4):
            qx, qy = _QUADRANT_OFFSETS[quadrant]
            cell0Based = cells[quadrant] - 1
            cellX, cellY = _cellTopLeftInArea(cell0Based)
            srcX = cellX + qx * _HALF + frameOffsetX
            srcY = cellY + qy * _HALF
            painter.drawImage(
                QtCore.QRect(qx * _HALF, qy * _HALF, _HALF, _HALF),
                source,
                QtCore.QRect(srcX, srcY, _HALF, _HALF),
            )
        painter.end()
        return out


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
    if y < 0 or y >= len(autoTiles):
        return 0
    row = autoTiles[y]
    if x < 0 or x >= len(row):
        return 0
    key = row[x]
    if key is None:
        return 0
    mask = 0
    height = len(autoTiles)
    width = len(row)

    def sameAt(nx: int, ny: int) -> bool:
        if 0 <= ny < height and 0 <= nx < width:
            r = autoTiles[ny]
            if isinstance(r, list) and 0 <= nx < len(r):
                return r[nx] == key
        return False

    if sameAt(x, y - 1):
        mask |= MASK_TOP
    if sameAt(x + 1, y):
        mask |= MASK_RIGHT
    if sameAt(x, y + 1):
        mask |= MASK_BOTTOM
    if sameAt(x - 1, y):
        mask |= MASK_LEFT
    if sameAt(x - 1, y - 1):
        mask |= MASK_TOP_LEFT
    if sameAt(x + 1, y - 1):
        mask |= MASK_TOP_RIGHT
    if sameAt(x + 1, y + 1):
        mask |= MASK_BOTTOM_RIGHT
    if sameAt(x - 1, y + 1):
        mask |= MASK_BOTTOM_LEFT
    return mask
