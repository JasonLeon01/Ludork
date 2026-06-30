# -*- encoding: utf-8 -*-

from typing import List, Optional
from Engine.Gameplay.Actors import Actor
from Global import System, GameMap


def _getCurrentGameMap() -> Optional[GameMap]:
    from Source.Scenes import Map as SceneMap

    scene = System.getScene()
    assert isinstance(scene, SceneMap)
    return scene.getGameMap()


@Meta(DisplayName='LOC("GET_ACTOR_BY_TAG")', DisplayDesc='LOC("GET_ACTOR_BY_TAG_DESC")')
@ReturnType(actor=Optional[Actor])
def GetActorByTag(tag: str) -> Optional[Actor]:
    r"""\brief Find an actor by tag on the current map.

    - \param tag The tag to search for.
    - \return The matching actor, or None if not found.
    """
    gameMap = _getCurrentGameMap()
    if gameMap is None:
        return None
    return gameMap.getActorByTag(tag)


@Meta(DisplayName='LOC("GET_ALL_ACTORS")', DisplayDesc='LOC("GET_ALL_ACTORS_DESC")')
@ReturnType(actors=List[Actor])
def GetAllActors() -> List[Actor]:
    r"""\brief Get all actors on the current map.

    - \return A flat list of all actors across all layers.
    """
    gameMap = _getCurrentGameMap()
    if gameMap is None:
        return []
    return gameMap.getAllActors()
