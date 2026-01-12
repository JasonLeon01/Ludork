# -*- encoding: utf-8 -*-

from __future__ import annotations
from . import A_Base
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from . import Vector2f, Vector2i, Vector2u, GetCellSize, ExecSplit, ReturnType
from ...Utils import Math

if TYPE_CHECKING:
    from Engine import Texture, IntRect

ActorBase = A_Base.ActorBase


class Actor(ActorBase):
    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]] = None,
        tag: str = "",
    ) -> None:
        super().__init__(texture, rect, tag)
        self._collisionEnabled: bool = False
        self._tickable: bool = False
        self._speed: float = 64.0
        self._isMoving: bool = False
        self._nextMoveOffset: Optional[Union[Vector2i, Tuple[int, int]]] = None
        self._inRoutine: bool = False
        self._routine: Optional[List[Vector2i]] = None
        self._departure: Optional[Vector2f] = None
        self._destination: Optional[Vector2f] = None
        self._realSpeed: float = 0.0

    def fixedUpdate(self, fixedDelta: float) -> None:
        startPosition = self.getPosition()
        if self._inRoutine:
            if len(self._routine) == 0:
                self._inRoutine = False
            else:
                if not self._isMoving:
                    step = self._routine[0]
                    self._routine.pop(0)
                    if not self.MapMove(step):
                        self._inRoutine = False
        if self._isMoving:
            self._processMoving(fixedDelta)
        dist = (self.getPosition() - startPosition).length()
        if fixedDelta <= 0.0 or Math.IsNearZero(dist, 0.001):
            self._realSpeed = 0.0
        else:
            self._realSpeed = dist / fixedDelta

    def onCreate(self) -> None:
        pass

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        pass

    def onDestroy(self) -> None:
        pass

    def onCollision(self, other: List[Actor]) -> None:
        pass

    def onOverlap(self, other: List[Actor]) -> None:
        pass

    @ExecSplit(default=(None,))
    def destroy(self) -> None:
        self._map.destroyActor(self)

    @ExecSplit(success=(True,), fail=(False,))
    def MapMove(self, offset: Union[Vector2i, Tuple[int, int]]) -> None:
        assert isinstance(offset, (Vector2i, tuple)), "offset must be a tuple or Vector2i"
        if not isinstance(offset, Vector2i):
            x, y = offset
            offset = Vector2i(x, y)
        x = offset.x
        y = offset.y
        sx = 1 if x > 0 else (-1 if x < 0 else 0)
        sy = 1 if y > 0 else (-1 if y < 0 else 0)
        offset = Vector2i(sx, sy)
        if offset == Vector2i(0, 0):
            return False
        if self._isMoving:
            return False
        target = self.getMapPosition() + offset
        if target.x < 0 or target.x >= self._map.getSize().x or target.y < 0 or target.y >= self._map.getSize().y:
            return False
        if not self._map.isPassable(self, target):
            collisions = self._map.getCollision(self, target)
            if collisions:
                Actor.ActorCollision(self, collisions)
                for collision in collisions:
                    Actor.ActorCollision(collision, [self])
            return False
        self._isMoving = True
        self._departure = self.getPosition()
        self._destination = Vector2f(
            self.getPosition().x + offset.x * GetCellSize(),
            self.getPosition().y + offset.y * GetCellSize(),
        )
        return True

    @ReturnType(tickable=bool)
    def getTickable(self) -> bool:
        return self._tickable

    @ExecSplit(default=(None,))
    def setTickable(self, tickable: bool, applyToChildren: bool = True) -> None:
        self._tickable = tickable
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    if isinstance(child, Actor):
                        child.setTickable(tickable, applyToChildren)

    @ReturnType(visible=bool)
    def getVisible(self) -> bool:
        return self._visible

    @ExecSplit(default=(None,))
    def setVisible(self, visible: bool, applyToChildren: bool = True) -> None:
        self._visible = visible
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setVisible(visible, applyToChildren)

    @ReturnType(collisionEnabled=bool)
    def getCollisionEnabled(self) -> bool:
        return self._collisionEnabled

    @ExecSplit(default=(None,))
    def setCollisionEnabled(self, enabled: bool) -> None:
        self._collisionEnabled = enabled

    @ReturnType(intersects=bool)
    def intersects(self, other: Actor) -> bool:
        return self.getGlobalBounds().findIntersection(other.getGlobalBounds()) != None

    @ReturnType(isMoving=bool)
    def isMoving(self) -> bool:
        return self._isMoving or self._realSpeed > 0.0

    @ReturnType(isInRoutine=bool)
    def isInRoutine(self) -> bool:
        return self._inRoutine

    @ExecSplit(default=(None,))
    def setRoutine(self, routine: Optional[List[Vector2i]]) -> None:
        self._inRoutine = routine is not None
        self._routine = routine

    @ReturnType(routine=Optional[List[Vector2i]])
    def getRoutine(self) -> Optional[List[Vector2i]]:
        return self._routine

    @ExecSplit(default=(None,))
    def stop(self) -> None:
        self._isMoving = False
        self._inRoutine = False
        self._routine = None
        self._departure = None
        self._destination = None
        self._realSpeed = 0.0
        self._autoFixMapPosition()

    @ReturnType(velocity=Optional[Vector2f])
    def getVelocity(self) -> Optional[Vector2f]:
        if self._departure is None or self._destination is None:
            return None

        dist = self._destination - self._departure
        length = dist.length()
        time = length / self._speed
        velocity = dist / time
        return velocity

    @staticmethod
    def ActorCreate(actor: Actor) -> None:
        if (
            hasattr(type(actor), "GENERATED_CLASS")
            and type(actor).GENERATED_CLASS
            and not actor._graph is None
            and actor._graph.hasKey("onCreate")
        ):
            actor._graph.execute("onCreate")
        else:
            actor.onCreate()

    @staticmethod
    def ActorTick(actor: Actor, deltaTime: float) -> None:
        if (
            hasattr(type(actor), "GENERATED_CLASS")
            and type(actor).GENERATED_CLASS
            and not actor._graph is None
            and actor._graph.hasKey("onTick")
        ):
            actor._graph.localGraph["__deltaTime__"] = deltaTime
            actor._graph.execute("onTick")
        else:
            actor.onTick(deltaTime)

    @staticmethod
    def ActorLateTick(actor: Actor, deltaTime: float) -> None:
        if (
            hasattr(type(actor), "GENERATED_CLASS")
            and type(actor).GENERATED_CLASS
            and not actor._graph is None
            and actor._graph.hasKey("onLateTick")
        ):
            actor._graph.localGraph["__deltaTime__"] = deltaTime
            actor._graph.execute("onLateTick")
        else:
            actor.onLateTick(deltaTime)

    @staticmethod
    def ActorFixedTick(actor: Actor, fixedDelta: float) -> None:
        if (
            hasattr(type(actor), "GENERATED_CLASS")
            and type(actor).GENERATED_CLASS
            and not actor._graph is None
            and actor._graph.hasKey("onFixedTick")
        ):
            actor._graph.localGraph["__fixedDeltaTime__"] = fixedDelta
            actor._graph.execute("onFixedTick")
        else:
            actor.onFixedTick(fixedDelta)

    @staticmethod
    def ActorDestroy(actor: Actor) -> None:
        if (
            hasattr(type(actor), "GENERATED_CLASS")
            and type(actor).GENERATED_CLASS
            and not actor._graph is None
            and actor._graph.hasKey("onDestroy")
        ):
            actor._graph.execute("onDestroy")
        else:
            actor.onDestroy()

    @staticmethod
    def ActorCollision(actor: Actor, other: List[Actor]) -> None:
        if (
            hasattr(type(actor), "GENERATED_CLASS")
            and getattr(type(actor), "GENERATED_CLASS")
            and not actor._graph is None
            and actor._graph.hasKey("onCollision")
        ):
            actor._graph.localGraph["__onCollision__"] = [other]
            actor._graph.execute("onCollision")
        else:
            actor.onCollision(other)

    @staticmethod
    def ActorOverlap(actor: Actor, other: List[Actor]) -> None:
        if (
            hasattr(type(actor), "GENERATED_CLASS")
            and getattr(type(actor), "GENERATED_CLASS")
            and not actor._graph is None
            and actor._graph.hasKey("onOverlap")
        ):
            actor._graph.localGraph["__onOverlap__"] = [other]
            actor._graph.execute("onOverlap")
        else:
            actor.onOverlap(other)

    def _processMoving(self, fixedDelta: float) -> None:
        velocity = self.getVelocity()
        if velocity is None:
            return
        offset = velocity * fixedDelta
        self.move(offset)
        if (
            Math.IsNearZero(self.getPosition().x - self._destination.x, 0.1)
            and Math.IsNearZero(self.getPosition().y - self._destination.y, 0.1)
        ) or (
            (self._destination.x - self._departure.x) * (self.getPosition().x - self._destination.x) > 0
            or (self._destination.y - self._departure.y) * (self.getPosition().y - self._destination.y) > 0
        ):
            self.setPosition(Vector2f(self._destination.x, self._destination.y))
            self._autoFixMapPosition()
            self._isMoving = False
            self._departure = None
            self._destination = None
            overlaps = self._map.getOverlaps(self)
            if overlaps:
                Actor.ActorOverlap(self, overlaps)
                for overlap in overlaps:
                    Actor.ActorOverlap(overlap, [self])

    def _autoFixMapPosition(self) -> None:
        pos = self.getPosition()
        x = int(pos.x * 1.0 / GetCellSize() + 0.5)
        y = int(pos.y * 1.0 / GetCellSize() + 0.5)
        self.setMapPosition(Vector2u(x, y))
        if self._map:
            self._map.markPassabilityDirty()
