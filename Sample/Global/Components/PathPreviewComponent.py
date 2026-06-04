# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TYPE_CHECKING
import Engine
from Engine import RectangleShape, Vector2f, Color
from .ComponentBase import ComponentBase
from .PathRouteState import PathRouteState

if TYPE_CHECKING:
    from ..GameMap import GameMap
    from ..Camera import Camera


class PathPreviewComponent(ComponentBase):
    r"""
    \brief Component that renders a visual preview of the pathfinding route.

    This component renders semi-transparent rectangles along the
    planned path to show where the actor will move.
    """

    def __init__(self, gameMap: GameMap, routeState: PathRouteState) -> None:
        r"""
        \brief Initialize the PathPreviewComponent.

        - gameMap: The game map this component operates on.
        - routeState: The path route state to read the planned path from.
        """
        super().__init__(gameMap)
        self._routeState = routeState
        self._fillColor = Color(80, 180, 255, 110)
        self._outlineColor = Color(120, 220, 255, 180)

    def onRender(self, camera: Camera) -> None:
        r"""
        \brief Render the path preview on the map.

        - camera: The camera to use for rendering.
        """
        route = self._routeState.getRoute()
        if len(route) == 0:
            return
        cellSize = Engine.CellSize
        pad = max(1.0, cellSize * 0.12)
        size = max(1.0, cellSize - pad * 2.0)
        for cell in route:
            rect = RectangleShape(Vector2f(size, size))
            rect.setPosition(Vector2f(cell.x * cellSize + pad, cell.y * cellSize + pad))
            rect.setFillColor(self._fillColor)
            rect.setOutlineColor(self._outlineColor)
            rect.setOutlineThickness(1.0)
            camera.render(rect)
