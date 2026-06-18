# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Callable, List, Optional, Union
from Engine import Pair, Vector2i
from Engine.Gameplay.Actors import Actor


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
    if hasattr(value, "x") and hasattr(value, "y"):
        return Vector2i(int(value.x), int(value.y))
    if isinstance(value, (tuple, list)) and len(value) >= 2:
        return Vector2i(int(value[0]), int(value[1]))
    return None


def _normaliseRoute(route: Optional[List[Any]]) -> Optional[List[Vector2i]]:
    if route is None:
        return None
    result: List[Vector2i] = []
    for item in route:
        step = _toVector2i(item)
        if step is not None:
            result.append(step)
    return result


def _isUnitStep(value: Vector2i) -> bool:
    return abs(value.x) <= 1 and abs(value.y) <= 1 and not (value.x == 0 and value.y == 0)


def _routeFromOffsets(start: Vector2i, goal: Vector2i, rawPath: List[Vector2i]) -> Optional[List[Vector2i]]:
    if not all(_isUnitStep(step) for step in rawPath):
        return None
    current = Vector2i(start.x, start.y)
    for step in rawPath:
        current = Vector2i(current.x + step.x, current.y + step.y)
    if current != goal:
        return None
    return [Vector2i(step.x, step.y) for step in rawPath]


def _routeFromPoints(start: Vector2i, goal: Vector2i, rawPath: List[Vector2i]) -> Optional[List[Vector2i]]:
    points = [Vector2i(point.x, point.y) for point in rawPath]
    if points and points[0] == start:
        points = points[1:]
    if not points:
        return [] if start == goal else None
    if points[-1] != goal:
        return None
    current = Vector2i(start.x, start.y)
    route: List[Vector2i] = []
    for point in points:
        step = Vector2i(point.x - current.x, point.y - current.y)
        if not _isUnitStep(step):
            return None
        route.append(step)
        current = point
    return route


def _buildRouteToDestination(actor: Actor, destination: Vector2i) -> List[Vector2i]:
    gameMap = actor.getMap()
    if gameMap is None:
        return []
    start = actor.getMapPosition()
    goal = Vector2i(destination.x, destination.y)
    if start == goal:
        return []
    rawPath = [_toVector2i(point) for point in gameMap.findPath(start, goal)]
    rawPath = [point for point in rawPath if point is not None]
    if not rawPath:
        return []
    byOffset = _routeFromOffsets(start, goal, rawPath)
    byPoint = _routeFromPoints(start, goal, rawPath)
    candidates = [route for route in (byOffset, byPoint) if route is not None]
    if not candidates:
        return []
    return min(candidates, key=len)


def _isMovementFinished(actor: Optional[Actor]) -> bool:
    if actor is None:
        return True
    if hasattr(actor, "isDestroyed") and actor.isDestroyed():
        return True
    return not actor.isMoving() and not actor.isInRoute()


@Meta(DisplayName='LOC("SET_MOVE_ROUTE")', DisplayDesc='LOC("SET_MOVE_ROUTE_DESC")', MoveRouteVars=["route"])
@Latent(Started=(_LATENT_STARTED,), Finished=(_LATENT_FINISHED,))
def SetMoveRoute(actor: Actor, route: Optional[List[Any]] = None) -> Callable[[], List[int]]:
    r"""\brief Set an actor route and wait until movement finishes.

    - \param actor The actor to move.
    - \param route List of grid offsets, or `None` to clear the route.
    - \return A condition callable that emits Started immediately and Finished after movement ends.
    """
    if actor is not None:
        actor.setRoute(_normaliseRoute(route))
    return _MovementCondition(actor)


@Meta(DisplayName='LOC("SET_AUTO_PATH_TO_DESTINATION")', DisplayDesc='LOC("SET_AUTO_PATH_TO_DESTINATION_DESC")', Vector2iVars=["destination"])
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
    if actor is not None and target is not None:
        actor.setRoute(_buildRouteToDestination(actor, target))
    return _MovementCondition(actor)
