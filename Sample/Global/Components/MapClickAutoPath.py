# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
from threading import Lock
from typing import Any, Dict, List, Optional, TYPE_CHECKING
import Engine
from Engine import Vector2i, Vector2u, Direction, Input, Mouse
from Engine.Gameplay.Actors import Actor
from ..System import System
from .ComponentBase import ComponentBase
from .PathRouteState import PathRouteState

if TYPE_CHECKING:
    from ..GameMap import GameMap


class MapClickAutoPath(ComponentBase):
    r"""
    \brief Component that pathfinds and moves an actor to the clicked tile.

    This component listens for mouse clicks, finds a path to the
    clicked tile, and moves the player actor along that path.
    """

    def __init__(self, gameMap: GameMap, routeState: PathRouteState) -> None:
        r"""
        \brief Initialize the MapClickAutoPath component.

        - gameMap: The game map this component operates on.
        - routeState: The path route state to use for path preview.
        """
        super().__init__(gameMap)
        self._routeState = routeState
        self._autoPathing: bool = False
        self._pendingGoals: deque[Vector2i] = deque()
        self._pendingGoalsLock = Lock()

    def onLateTick(self) -> None:
        r"""
        \brief Handle mouse click input to set pathfinding goals.

        This method checks for left mouse button clicks and adds
        the clicked map position to the pending goals queue.
        """
        player = self._parent.getPlayer()
        if player is None or self._isAutoPathBlocked(player):
            return
        if Input.isMouseButtonTriggered(Mouse.Button.Left, True):
            goal = self._getMouseMapPosition()
            if goal is not None:
                with self._pendingGoalsLock:
                    self._pendingGoals.append(goal)
            return
        if Input.isTouchBegan(handled=True):
            goal = self._getTouchMapPosition()
            if goal is not None:
                with self._pendingGoalsLock:
                    self._pendingGoals.append(goal)

    def onTick(self) -> None:
        r"""
        \brief Update pathfinding and movement logic.

        This method processes pending goals, builds pathfinding plans,
        and moves the player actor along the calculated paths.
        """
        gameMap: GameMap = self._parent
        player = gameMap.getPlayer()
        if player is None or self._isAutoPathBlocked(player):
            self._autoPathing = False
            self._routeState.clear()
            with self._pendingGoalsLock:
                self._pendingGoals.clear()
            return
        currentPos = player.getMapPosition()
        self._trimPreviewRoute(currentPos)
        if self._autoPathing and not player.isMoving() and not player.isInRoute():
            self._autoPathing = False
            self._routeState.clear()
        goals = self._drainPendingGoals()
        if len(goals) == 0:
            return
        if self._autoPathing:
            self._finishAutoPathImmediately(player, currentPos)
            self._autoPathing = False
            self._routeState.clear()
            return
        for goal in goals:
            if not self._isInMap(goal):
                continue
            start = player.getMapPosition()
            if start == goal:
                self._routeState.clear()
                self._setActorDirection(player, start, goal, True)
                continue
            plan = self._buildAutoPathPlan(player, start, goal)
            if plan is None:
                continue
            self._routeState.setRoute(plan["route"])
            self._trimPreviewRoute(start)
            player.setRoute(plan["routeSteps"])
            self._autoPathing = True

    def _finishAutoPathImmediately(
        self,
        player: Actor,
        start: Vector2i,
    ) -> None:
        route = self._routeState.getRoute()
        if len(route) == 0:
            player.stop()
            return
        goal = route[-1]
        goalPassable = self._parent.isPassable(player, goal)
        player.stop()
        destination = goal if goalPassable else (route[-2] if len(route) >= 2 else start)
        walkCount = self._getInstantWalkCount(route, destination)
        player.setMapPosition(Vector2u(destination.x, destination.y))
        self._dispatchInstantMoveOverlaps(player)
        self._triggerInstantWalkStates(player, walkCount)
        fromPos = route[-2] if len(route) >= 2 else start
        self._setActorDirection(player, fromPos, goal, start == goal)
        if not goalPassable and destination != goal:
            bumpOffset = Vector2i(goal.x - destination.x, goal.y - destination.y)
            player.MapMove(bumpOffset)

    def _getInstantWalkCount(self, route: List[Vector2i], destination: Vector2i) -> int:
        for index, point in enumerate(route):
            if point == destination:
                return index + 1
        return 0

    def _triggerInstantWalkStates(self, player: Actor, walkCount: int) -> None:
        if walkCount <= 0:
            return
        triggerStateWalk = getattr(player, "triggerStateWalk", None)
        if not callable(triggerStateWalk):
            return
        for _ in range(walkCount):
            triggerStateWalk()

    def _dispatchInstantMoveOverlaps(self, player: Actor) -> None:
        overlaps = self._parent.getOverlaps(player)
        if not overlaps:
            return
        Actor.BlueprintEvent(player, Actor, "onOverlap", {"other": overlaps})
        for overlap in overlaps:
            Actor.BlueprintEvent(overlap, Actor, "onOverlap", {"other": [player]})

    def _buildAutoPathPlan(
        self,
        actor: Actor,
        start: Vector2i,
        goal: Vector2i,
    ) -> Optional[Dict[str, Any]]:
        if start == goal:
            return {"routeSteps": [], "route": [Vector2i(start.x, start.y)], "goalPassable": True}
        gameMap = self._parent
        goalPassable = gameMap.isPathfindingPassable(actor, goal)
        goalActuallyPassable = gameMap.isPassable(actor, goal)
        direct = self._buildPathToTarget(start, goal)
        if goalPassable and direct is not None and direct["route"][-1] == goal:
            return {"routeSteps": direct["routeSteps"], "route": direct["route"], "goalPassable": True}
        if goalPassable:
            return None
        bestPlan: Optional[Dict[str, Any]] = None
        for offset in (Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0)):
            neighbour = Vector2i(goal.x + offset.x, goal.y + offset.y)
            if not self._isInMap(neighbour):
                continue
            if neighbour != start and not gameMap.isPathfindingPassable(actor, neighbour):
                continue
            if neighbour == start:
                routeSteps = []
                route = [Vector2i(start.x, start.y)]
            else:
                neighbourPlan = self._buildPathToTarget(start, neighbour)
                if neighbourPlan is None or neighbourPlan["route"][-1] != neighbour:
                    continue
                routeSteps = neighbourPlan["routeSteps"]
                route = neighbourPlan["route"]
            if bestPlan is None or len(route) < len(bestPlan["route"]):
                bestPlan = {"routeSteps": routeSteps, "route": route}
        if bestPlan is None:
            return None
        if goalActuallyPassable:
            return {"routeSteps": bestPlan["routeSteps"], "route": bestPlan["route"], "goalPassable": False}
        fullRoute = [Vector2i(p.x, p.y) for p in bestPlan["route"]]
        fullRoute.append(Vector2i(goal.x, goal.y))
        routeSteps = [Vector2i(p.x, p.y) for p in bestPlan["routeSteps"]]
        stopPos = fullRoute[-2]
        routeSteps.append(Vector2i(goal.x - stopPos.x, goal.y - stopPos.y))
        return {"routeSteps": routeSteps, "route": fullRoute, "goalPassable": False}

    def _buildPathToTarget(self, start: Vector2i, target: Vector2i) -> Optional[Dict[str, List[Vector2i]]]:
        rawPath = self._parent.findPath(start, target)
        candidates: List[List[Vector2i]] = []
        routeStepsFromOffset = self._convertPathAsOffset(rawPath)
        if routeStepsFromOffset is not None:
            candidates.append(routeStepsFromOffset)
        routeStepsFromPoint = self._convertPathAsPoint(start, rawPath)
        if routeStepsFromPoint is not None:
            candidates.append(routeStepsFromPoint)
        if len(candidates) == 0:
            return None
        best: Optional[Dict[str, List[Vector2i]]] = None
        for routeSteps in candidates:
            route = self._buildRouteBySteps(start, routeSteps)
            if len(route) == 0:
                continue
            item = {"routeSteps": routeSteps, "route": route}
            if route[-1] == target:
                if best is None or best["route"][-1] != target or len(route) < len(best["route"]):
                    best = item
            elif best is None:
                best = item
        return best

    def _convertPathAsOffset(self, rawPath: List[Vector2i]) -> Optional[List[Vector2i]]:
        routeSteps: List[Vector2i] = []
        for step in rawPath:
            sx = 1 if step.x > 0 else (-1 if step.x < 0 else 0)
            sy = 1 if step.y > 0 else (-1 if step.y < 0 else 0)
            if sx != 0 and sy != 0:
                return None
            count = abs(step.x) + abs(step.y)
            if count == 0:
                continue
            for _ in range(count):
                routeSteps.append(Vector2i(sx, sy))
        return routeSteps

    def _convertPathAsPoint(self, start: Vector2i, rawPath: List[Vector2i]) -> Optional[List[Vector2i]]:
        points: List[Vector2i] = [Vector2i(p.x, p.y) for p in rawPath]
        if len(points) > 0 and points[0] == start:
            points = points[1:]
        current = Vector2i(start.x, start.y)
        routeSteps: List[Vector2i] = []
        for point in points:
            dx = point.x - current.x
            dy = point.y - current.y
            if dx != 0 and dy != 0:
                return None
            steps = abs(dx) + abs(dy)
            if steps == 0:
                continue
            sx = 1 if dx > 0 else (-1 if dx < 0 else 0)
            sy = 1 if dy > 0 else (-1 if dy < 0 else 0)
            for _ in range(steps):
                routeSteps.append(Vector2i(sx, sy))
                current = Vector2i(current.x + sx, current.y + sy)
        return routeSteps

    def _buildRouteBySteps(self, start: Vector2i, routeSteps: List[Vector2i]) -> List[Vector2i]:
        route: List[Vector2i] = [Vector2i(start.x, start.y)]
        current = Vector2i(start.x, start.y)
        for step in routeSteps:
            sx = 1 if step.x > 0 else (-1 if step.x < 0 else 0)
            sy = 1 if step.y > 0 else (-1 if step.y < 0 else 0)
            if sx != 0 and sy != 0:
                return route
            if sx == 0 and sy == 0:
                continue
            current = Vector2i(current.x + sx, current.y + sy)
            route.append(current)
        return route

    def _getMouseMapPosition(self) -> Optional[Vector2i]:
        mousePos = Input.getMousePosition()
        scale = System.getScale()
        if scale > 0:
            mousePos = Vector2i(int(mousePos.x / scale), int(mousePos.y / scale))
        mousePos = self._toMapViewPixelPosition(mousePos)
        worldPos = self._parent.getCamera().mapPixelToCoords(mousePos)
        cellSize = Engine.CellSize
        return Vector2i(int(worldPos.x // cellSize), int(worldPos.y // cellSize))

    def _getTouchMapPosition(self) -> Optional[Vector2i]:
        touchPos = Input.getTouchBeganPosition()
        if touchPos is None:
            return None
        scale = System.getScale()
        if scale > 0:
            touchPos = Vector2i(int(touchPos.x / scale), int(touchPos.y / scale))
        touchPos = self._toMapViewPixelPosition(touchPos)
        worldPos = self._parent.getCamera().mapPixelToCoords(touchPos)
        cellSize = Engine.CellSize
        return Vector2i(int(worldPos.x // cellSize), int(worldPos.y // cellSize))

    def _toMapViewPixelPosition(self, pos: Vector2i) -> Vector2i:
        mapOffset = self._parent.getMapViewOffset()
        return Vector2i(int(pos.x - mapOffset.x), int(pos.y - mapOffset.y))

    def _isInMap(self, pos: Vector2i) -> bool:
        size = self._parent.getSize()
        return 0 <= pos.x < size.x and 0 <= pos.y < size.y

    def _setActorDirection(self, actor: Actor, fromPos: Vector2i, goal: Vector2i, rotateWhenSame: bool) -> None:
        if not hasattr(actor, "direction"):
            return
        if rotateWhenSame:
            actor.direction = Direction((int(actor.direction) + 1) % 4)
            return
        dx = goal.x - fromPos.x
        dy = goal.y - fromPos.y
        if dx == 0 and dy == 0:
            return
        if abs(dx) > abs(dy):
            actor.direction = Direction.RIGHT if dx > 0 else Direction.LEFT
        else:
            actor.direction = Direction.DOWN if dy > 0 else Direction.UP

    def _drainPendingGoals(self) -> List[Vector2i]:
        with self._pendingGoalsLock:
            goals = list(self._pendingGoals)
            self._pendingGoals.clear()
        return goals

    def _isAutoPathBlocked(self, player: Actor) -> bool:
        if not player.getMoveEnabled():
            return True
        getForbiddenMoving = getattr(player, "getForbiddenMoving", None)
        if callable(getForbiddenMoving) and getForbiddenMoving():
            return True
        scene = self._parent.getScene()
        return scene is not None and scene.isInputBlocked()

    def _trimPreviewRoute(self, currentPos: Vector2i) -> None:
        route = self._routeState.getRoute()
        if len(route) == 0:
            return
        index = -1
        for i, point in enumerate(route):
            if point == currentPos:
                index = i
                break
        if index >= 0:
            route = route[index + 1 :]
            self._routeState.setRoute(route)
