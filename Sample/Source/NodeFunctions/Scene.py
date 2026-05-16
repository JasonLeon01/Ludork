# -*- encoding: utf-8 -*-
r"""\brief Blueprint scene nodes: map navigation and scene-level shortcuts."""

from typing import Optional, Union
from Engine import Vector2u
from Global import System


@Meta(DisplayName='LOC("GOTO_MAP")', DisplayDesc='LOC("GOTO_MAP_DESC")')
@ExecSplit(default=(None,))
def GotoMap(mapPath: str, x: Optional[int] = None, y: Optional[int] = None) -> None:
    r"""\brief Transition to a map and optionally place the player at (x, y).

    - \param mapPath Path to the map data file (relative to Data/Maps/).
    - \param x Target tile column, or None to keep the current position.
    - \param y Target tile row, or None to keep the current position.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "gotoMapAndPos"):
        pos = Vector2u(int(x), int(y)) if x is not None and y is not None else None
        scene.gotoMapAndPos(mapPath, pos)
