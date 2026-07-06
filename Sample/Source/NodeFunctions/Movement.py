# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Callable, List, Optional, Union
from Engine import Pair, Vector2f, Vector2i, Vector2u
from Engine.Gameplay.Actors import Actor
from Source.Player import Player


_LATENT_STARTED = 0
_LATENT_FINISHED = 1


class _MovementCondition:
    def __init__(self, actor: Optional[Actor]) -> None:
        self._actor = actor
        self._startedEmitted = False
        self._finished = False

    def __call__(self) -> List[int]:
        if not self._startedEmitted:
            self._startedEmitted = True
            return [_LATENT_STARTED]
        if _isMovementFinished(self._actor):
            self._finished = True
            return [_LATENT_FINISHED]
        return []

    def isFinished(self) -> bool:
        return self._finished


def _toVector2i(value: Any) -> Optional[Vector2i]:
    if isinstance(value, Vector2i):
        return Vector2i(value.x, value.y)
    if isinstance(value, (Vector2f, Vector2u)):
        return Vector2i(int(value.x), int(value.y))
    if isinstance(value, (tuple, list)) and len(value) >= 2:
        return Vector2i(int(value[0]), int(value[1]))
    return None


def _buildRouteToDestination(actor: Actor, destination: Vector2i) -> List[Vector2i]:
    gameMap = actor.getMap()
    if gameMap is None:
        return []
    start = actor.getMapPosition()
    goal = Vector2i(destination.x, destination.y)
    if start == goal:
        return []
    pathResult = gameMap.findPathResult(start, goal)
    if len(pathResult.route) == 0 or pathResult.route[-1] != goal:
        return []
    return pathResult.offsets


def _isMovementFinished(actor: Optional[Actor]) -> bool:
    if actor is None:
        return True
    if actor.isDestroyed():
        return True
    return not actor.isMoving() and not actor.isInRoute()


def _isMovementBlocked(actor: Optional[Actor]) -> bool:
    if actor is None:
        return True
    if isinstance(actor, Player) and actor.getForbiddenMoving():
        return True
    gameMap = actor.getMap()
    if gameMap is None:
        return False
    scene = gameMap.getScene()
    return scene is not None and scene.isInputBlocked()


@Meta(DisplayName='LOC("SET_MOVE_ENABLED_BY_TAG")', DisplayDesc='LOC("SET_MOVE_ENABLED_BY_TAG_DESC")')
@ExecSplit(default=(None,))
def SetMoveEnabledByTag(tag: str, enabled: bool = True) -> None:
    r"""\brief Enable or disable movement for an actor identified by tag.

    - \param tag The tag of the actor to modify.
    - \param enabled Whether to enable movement.
    """
    from Source.Scenes import Map as SceneMap
    from Global import System

    scene = System.getScene()
    assert isinstance(scene, SceneMap)
    actor = scene.getGameMap().getActorByTag(tag)
    if actor is not None:
        actor.setMoveEnabled(enabled)


@Meta(DisplayName='LOC("SET_MOVE_ROUTE")', DisplayDesc='LOC("SET_MOVE_ROUTE_DESC")', MoveRouteVars=["route"])
@Latent(Started=(_LATENT_STARTED,), Finished=(_LATENT_FINISHED,))
def SetMoveRoute(actor: Actor, route: List[Vector2i] = []) -> Callable[[], List[int]]:
    r"""\brief Set an actor route and wait until movement finishes.

    - \param actor The actor to move.
    - \param route List of grid offsets, or `None` to clear the route.
    - \return A condition callable that emits Started immediately and Finished after movement ends.
    """
    if actor is not None and not _isMovementBlocked(actor):
        actor.setRoute(route)
    return _MovementCondition(actor)


@Meta(
    DisplayName='LOC("SET_AUTO_PATH_TO_DESTINATION")',
    DisplayDesc='LOC("SET_AUTO_PATH_TO_DESTINATION_DESC")',
    Vector2iVars=["destination"],
)
@Latent(Started=(_LATENT_STARTED,), Finished=(_LATENT_FINISHED,))
def SetAutoPathToDestination(
    actor: Actor, destination: Union[Vector2i, Pair[int], List[int]] = (0, 0)
) -> Callable[[], List[int]]:
    r"""\brief Pathfind an actor to a destination and wait until movement finishes.

    - \param actor The actor to move.
    - \param destination Target map position.
    - \return A condition callable that emits Started immediately and Finished after movement ends.
    """
    target = _toVector2i(destination)
    if actor is not None and target is not None and not _isMovementBlocked(actor):
        actor.setRoute(_buildRouteToDestination(actor, target))
    return _MovementCondition(actor)


@Meta(
    DisplayName='LOC("SET_AUTO_PATH_TO_DESTINATION_BY_TAG")',
    DisplayDesc='LOC("SET_AUTO_PATH_TO_DESTINATION_BY_TAG_DESC")',
    Vector2iVars=["destination"],
)
@Latent(Started=(_LATENT_STARTED,), Finished=(_LATENT_FINISHED,))
def SetAutoPathToDestinationByTag(
    tag: str, destination: Union[Vector2i, Pair[int], List[int]] = (0, 0)
) -> Callable[[], List[int]]:
    r"""\brief Pathfind an actor identified by tag to a destination and wait until movement finishes.

    - \param tag The tag of the actor to move.
    - \param destination Target map position.
    - \return A condition callable that emits Started immediately and Finished after movement ends.
    """
    from Source.Scenes import Map as SceneMap
    from Global import System

    actor = None
    scene = System.getScene()
    assert isinstance(scene, SceneMap)
    if tag:
        actor = scene.getGameMap().getActorByTag(tag)
    return SetAutoPathToDestination(actor, destination)
