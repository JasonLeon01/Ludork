# -*- encoding: utf-8 -*-

from __future__ import annotations

from typing import Any, Optional, Tuple, cast

from PyQt5 import QtCore, QtGui, QtWidgets

from EditorGlobal import EditorStatus
from .AutoTileRenderer import AutoTileRenderer
from .TilemapRenderer import TilemapRenderer


class MapRenderViewBase(QtWidgets.QWidget):
    def __init__(self, parent: Optional[QtWidgets.QWidget] = None) -> None:
        super(MapRenderViewBase, self).__init__(parent)
        self.setMouseTracking(True)
        self.setFocusPolicy(QtCore.Qt.ClickFocus)
        self._mapData: Optional[Any] = None
        self._pixmap: Optional[QtGui.QPixmap] = None
        self._tileSize = max(16, int(getattr(EditorStatus, "CELLSIZE", 32)))
        self._autoTileRenderer = AutoTileRenderer()
        self._tilemapRenderer = TilemapRenderer(self._autoTileRenderer)
        self.setMinimumSize(540, 360)
        self.setSizePolicy(QtWidgets.QSizePolicy.Expanding, QtWidgets.QSizePolicy.Expanding)

    def wheelEvent(self, event: QtGui.QWheelEvent) -> None:
        delta = event.angleDelta().y()
        if delta == 0:
            return
        step = 4 if delta > 0 else -4
        self._tileSize = max(12, min(96, self._tileSize + step))
        self._renderMap()
        self._updateContentSize()
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:
        painter = QtGui.QPainter(self)
        opt = QtWidgets.QStyleOption()
        opt.initFrom(self)
        style = cast(QtWidgets.QStyle, self.style())
        style.drawPrimitive(QtWidgets.QStyle.PE_Widget, opt, painter, self)
        offset = self._mapOffset()
        mapSize = self._mapPixelSize()
        rect = QtCore.QRect(offset, mapSize)
        painter.fillRect(rect, QtGui.QColor(38, 38, 38))
        if self._pixmap is not None:
            painter.drawPixmap(offset, self._pixmap)
        self._drawGrid(painter, offset)
        self._drawOverlay(painter, offset)
        painter.end()

    def _drawOverlay(self, painter: QtGui.QPainter, offset: QtCore.QPoint) -> None:
        pass

    def _renderMap(self) -> None:
        data = self._mapData
        if data is None:
            self._pixmap = None
            return
        if data.width <= 0 or data.height <= 0:
            self._pixmap = None
            return
        sourceTileSize = max(1, int(getattr(EditorStatus, "CELLSIZE", 32)))
        image = QtGui.QImage(data.width * self._tileSize, data.height * self._tileSize, QtGui.QImage.Format_ARGB32)
        image.fill(QtCore.Qt.transparent)
        painter = QtGui.QPainter(image)
        for layer in data.layers.values():
            if not isinstance(layer, dict):
                continue
            layerImg = self._tilemapRenderer.renderLayer(
                data.width,
                data.height,
                self._tileSize,
                layer.get("tiles"),
                layer.get("layerTileset"),
                layer.get("autoTiles"),
                0,
                sourceTileSize,
            )
            if layerImg is not None and not layerImg.isNull():
                painter.drawImage(0, 0, layerImg)
        painter.end()
        self._pixmap = QtGui.QPixmap.fromImage(image)

    def _drawGrid(self, painter: QtGui.QPainter, offset: QtCore.QPoint) -> None:
        data = self._mapData
        if data is None:
            return
        painter.save()
        painter.translate(offset)
        painter.setPen(QtGui.QPen(QtGui.QColor(255, 255, 255, 45), 1))
        for x in range(data.width + 1):
            px = x * self._tileSize
            painter.drawLine(px, 0, px, data.height * self._tileSize)
        for y in range(data.height + 1):
            py = y * self._tileSize
            painter.drawLine(0, py, data.width * self._tileSize, py)
        painter.restore()

    def _mapPixelSize(self) -> QtCore.QSize:
        if self._mapData is None:
            return QtCore.QSize(0, 0)
        return QtCore.QSize(self._mapData.width * self._tileSize, self._mapData.height * self._tileSize)

    def _mapOffset(self) -> QtCore.QPoint:
        size = self._mapPixelSize()
        return QtCore.QPoint(max(0, (self.width() - size.width()) // 2), max(0, (self.height() - size.height()) // 2))

    def _cellFromPos(self, pos: QtCore.QPoint) -> Optional[Tuple[int, int]]:
        data = self._mapData
        if data is None:
            return None
        local = pos - self._mapOffset()
        x = local.x() // self._tileSize
        y = local.y() // self._tileSize
        if not self._isInMap(x, y):
            return None
        return int(x), int(y)

    def _isInMap(self, x: int, y: int) -> bool:
        data = self._mapData
        return data is not None and 0 <= x < data.width and 0 <= y < data.height

    def _updateContentSize(self) -> None:
        size = self._mapPixelSize()
        if size.isValid() and not size.isEmpty():
            self.setMinimumSize(min(960, max(540, size.width())), min(615, max(360, size.height())))
