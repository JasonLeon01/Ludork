# -*- encoding: utf-8 -*-

from typing import Any

from Global import System


def _getSceneMap() -> Any:
    from Source.Scenes import Map as SceneMap

    scene = System.getScene()
    assert isinstance(scene, SceneMap)
    return scene


@Meta(DisplayName='LOC("OPEN_MONSTER_BOOK")', DisplayDesc='LOC("OPEN_MONSTER_BOOK_DESC")')
@ExecSplit(default=(None,))
def OpenMonsterBook() -> None:
    r"""\brief Open the current-map monster handbook."""
    _getSceneMap().showEnemyBook()


@Meta(DisplayName='LOC("OPEN_FLOOR_TELEPORTER")', DisplayDesc='LOC("OPEN_FLOOR_TELEPORTER_DESC")')
@ExecSplit(default=(None,))
def OpenFloorTeleporter() -> None:
    r"""\brief Open the visited-floor teleporter preview window."""
    _getSceneMap().showFloorTeleporter()


@Meta(DisplayName='LOC("GET_CURRENT_REGION")', DisplayDesc='LOC("GET_CURRENT_REGION_DESC")')
@ReturnType(region=str)
def GetCurrentRegion() -> str:
    r"""\brief Get the current mota region.

    - \return The current region, or an empty string when unavailable.
    """
    return _getSceneMap().inst.getCurrentRegion()


@Meta(DisplayName='LOC("SET_CURRENT_REGION")', DisplayDesc='LOC("SET_CURRENT_REGION_DESC")')
@ExecSplit(default=(None,))
def SetCurrentRegion(region: str) -> None:
    r"""\brief Set the current mota region.

    - \param region The region name to set.
    """
    _getSceneMap().inst.setCurrentRegion(region)
