# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TYPE_CHECKING
from Engine import RectangleShape, Vector2f, Color, GetCellSize
from .ComponentBase import ComponentBase
from .PathRouteState import PathRouteState

if TYPE_CHECKING:
    from ..GameMap import GameMap
    from ..Camera import Camera


class PathPreviewComponent(ComponentBase):
    def __init__(self, gameMap: GameMap, routeState: PathRouteState) -> None:
        super().__init__(gameMap)
        self._routeState = routeState
        self._fillColor = Color(80, 180, 255, 110)
        self._outlineColor = Color(120, 220, 255, 180)

    def onRender(self, camera: Camera) -> None:
        route = self._routeState.getRoute()
        if len(route) == 0:
            return
        cellSize = GetCellSize()
        pad = max(1.0, cellSize * 0.12)
        size = max(1.0, cellSize - pad * 2.0)
        for cell in route:
            rect = RectangleShape(Vector2f(size, size))
            rect.setPosition(Vector2f(cell.x * cellSize + pad, cell.y * cellSize + pad))
            rect.setFillColor(self._fillColor)
            rect.setOutlineColor(self._outlineColor)
            rect.setOutlineThickness(1.0)
            camera.render(rect)
