# -*- encoding: utf-8 -*-
r"""\brief Blueprint scene nodes: map navigation and scene-level shortcuts."""

from typing import Callable, List, Optional, Union
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


@Meta(DisplayName='LOC("OPEN_SHOP")', DisplayDesc='LOC("OPEN_SHOP_DESC")')
@Latent(Closed=(True,))
def OpenShop(items: List[str], canSell: bool) -> Callable[[], bool]:
    r"""\brief Open the map-bound shop.

    - \param items Item IDs available for purchase.
    - \param canSell Whether selling is available.
    - \return A condition callable that becomes True when the shop closes.
    """
    scene = System.getScene()
    if scene and hasattr(scene, "openShop"):
        return scene.openShop(list(items), bool(canSell))

    def condition() -> bool:
        return True

    return condition
