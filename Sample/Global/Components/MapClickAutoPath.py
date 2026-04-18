# -*- encoding: utf-8 -*-

from __future__ import annotations
from collections import deque
from threading import Lock
from typing import Any, Dict, List, Optional, TYPE_CHECKING
from Engine import Vector2i, Vector2u, Direction, Input, Mouse, GetCellSize
from Engine.Gameplay.Actors import Actor
from ..System import System
from .ComponentBase import ComponentBase
from .PathRouteState import PathRouteState

if TYPE_CHECKING:
    from ..GameMap import GameMap


class MapClickAutoPath(ComponentBase):
    def __init__(self, gameMap: GameMap, routeState: PathRouteState) -> None:
        super().__init__(gameMap)
        self._routeState = routeState
        self._autoPathing: bool = False
        self._pendingGoals: deque[Vector2i] = deque()
        self._pendingGoalsLock = Lock()

    def onLateTick(self) -> None:
        if not Input.isMouseButtonTriggered(Mouse.Button.Left, True):
            return
        goal = self._getMouseMapPosition()
        if goal is None:
            return
        with self._pendingGoalsLock:
            self._pendingGoals.append(goal)

    def onTick(self) -> None:
        gameMap: GameMap = self._parent
        player = gameMap.getPlayer()
        if player is None or not player.getMoveEnabled():
            self._autoPathing = False
            self._routeState.clear()
            with self._pendingGoalsLock:
                self._pendingGoals.clear()
            return
        currentPos = player.getMapPosition()
        self._trimPreviewRoute(currentPos)
        if self._autoPathing and not player.isMoving() and not player.isInRoutine():
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
            player.setRoutine(plan["routine"])
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
        player.setMapPosition(Vector2u(destination.x, destination.y))
        fromPos = route[-2] if len(route) >= 2 else start
        self._setActorDirection(player, fromPos, goal, start == goal)

    def _buildAutoPathPlan(
        self,
        actor: Actor,
        start: Vector2i,
        goal: Vector2i,
    ) -> Optional[Dict[str, Any]]:
        if start == goal:
            return {"routine": [], "route": [Vector2i(start.x, start.y)], "goalPassable": True}
        gameMap = self._parent
        goalPassable = gameMap.isPassable(actor, goal)
        direct = self._buildPathToTarget(start, goal)
        if goalPassable and direct is not None and direct["route"][-1] == goal:
            return {"routine": direct["routine"], "route": direct["route"], "goalPassable": True}
        if goalPassable:
            return None
        bestPlan: Optional[Dict[str, Any]] = None
        for offset in (Vector2i(0, 1), Vector2i(0, -1), Vector2i(1, 0), Vector2i(-1, 0)):
            neighbor = Vector2i(goal.x + offset.x, goal.y + offset.y)
            if not self._isInMap(neighbor):
                continue
            if neighbor != start and not gameMap.isPassable(actor, neighbor):
                continue
            if neighbor == start:
                routine = []
                route = [Vector2i(start.x, start.y)]
            else:
                neighborPlan = self._buildPathToTarget(start, neighbor)
                if neighborPlan is None or neighborPlan["route"][-1] != neighbor:
                    continue
                routine = neighborPlan["routine"]
                route = neighborPlan["route"]
            if bestPlan is None or len(route) < len(bestPlan["route"]):
                bestPlan = {"routine": routine, "route": route}
        if bestPlan is None:
            return None
        fullRoute = [Vector2i(p.x, p.y) for p in bestPlan["route"]]
        fullRoute.append(Vector2i(goal.x, goal.y))
        return {"routine": bestPlan["routine"], "route": fullRoute, "goalPassable": False}

    def _buildPathToTarget(self, start: Vector2i, target: Vector2i) -> Optional[Dict[str, List[Vector2i]]]:
        rawPath = self._parent.findPath(start, target)
        candidates: List[List[Vector2i]] = []
        routineFromOffset = self._convertPathAsOffset(rawPath)
        if routineFromOffset is not None:
            candidates.append(routineFromOffset)
        routineFromPoint = self._convertPathAsPoint(start, rawPath)
        if routineFromPoint is not None:
            candidates.append(routineFromPoint)
        if len(candidates) == 0:
            return None
        best: Optional[Dict[str, List[Vector2i]]] = None
        for routine in candidates:
            route = self._buildRouteByRoutine(start, routine)
            if len(route) == 0:
                continue
            item = {"routine": routine, "route": route}
            if route[-1] == target:
                if best is None or best["route"][-1] != target or len(route) < len(best["route"]):
                    best = item
            elif best is None:
                best = item
        return best

    def _convertPathAsOffset(self, rawPath: List[Vector2i]) -> Optional[List[Vector2i]]:
        routine: List[Vector2i] = []
        for step in rawPath:
            sx = 1 if step.x > 0 else (-1 if step.x < 0 else 0)
            sy = 1 if step.y > 0 else (-1 if step.y < 0 else 0)
            if sx != 0 and sy != 0:
                return None
            count = abs(step.x) + abs(step.y)
            if count == 0:
                continue
            for _ in range(count):
                routine.append(Vector2i(sx, sy))
        return routine

    def _convertPathAsPoint(self, start: Vector2i, rawPath: List[Vector2i]) -> Optional[List[Vector2i]]:
        points: List[Vector2i] = [Vector2i(p.x, p.y) for p in rawPath]
        if len(points) > 0 and points[0] == start:
            points = points[1:]
        current = Vector2i(start.x, start.y)
        routine: List[Vector2i] = []
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
                routine.append(Vector2i(sx, sy))
                current = Vector2i(current.x + sx, current.y + sy)
        return routine

    def _buildRouteByRoutine(self, start: Vector2i, routine: List[Vector2i]) -> List[Vector2i]:
        route: List[Vector2i] = [Vector2i(start.x, start.y)]
        current = Vector2i(start.x, start.y)
        for step in routine:
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
        worldPos = self._parent.getCamera().mapPixelToCoords(mousePos)
        cellSize = GetCellSize()
        return Vector2i(int(worldPos.x // cellSize), int(worldPos.y // cellSize))

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
