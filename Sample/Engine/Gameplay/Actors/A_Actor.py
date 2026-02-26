# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from ... import (
    TypeAdapter,
    Pair,
    BPBase,
    Vector2f,
    Vector2i,
    Vector2u,
    GetCellSize,
    ExecSplit,
    ReturnType,
    RegisterEvent,
    PathVars,
    RectRangeVars,
)
from ..G_Material import Material
from ...Utils import Math, Inner
from .A_Base import _ActorBase

if TYPE_CHECKING:
    from Engine import Texture, IntRect


@PathVars("texturePath")
@RectRangeVars(defaultRect="texturePath")
class Actor(_ActorBase, BPBase):
    collisionEnabled: bool = False
    tickable: bool = False
    speed: float = 64.0
    ### Generation use only
    texturePath: str = ""
    defaultRect: Optional[Tuple[Pair[int], Pair[int]]] = ((0, 0), (32, 32))
    defaultTranslation: Pair[float] = (0.0, 0.0)
    defaultRotation: float = 0.0
    defaultScale: Pair[float] = (1.0, 1.0)
    defaultOrigin: Pair[float] = (0.0, 0.0)
    ### Generation use only

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        super().__init__(texture, rect, tag)
        self.collisionEnabled: bool = False
        self.tickable: bool = False
        self.speed: float = 64.0
        self._isMoving: bool = False
        self._nextMoveOffset: Optional[Union[Vector2i, Pair[int]]] = None
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

    @RegisterEvent
    def onCreate(self) -> None:
        pass

    @RegisterEvent
    def onTick(self, deltaTime: float) -> None:
        pass

    @RegisterEvent
    def onLateTick(self, deltaTime: float) -> None:
        pass

    @RegisterEvent
    def onFixedTick(self, fixedDelta: float) -> None:
        pass

    @RegisterEvent
    def onDestroy(self) -> None:
        pass

    @RegisterEvent
    def onCollision(self, other: List[Actor]) -> None:
        pass

    @RegisterEvent
    def onOverlap(self, other: List[Actor]) -> None:
        pass

    @ExecSplit(default=(None,))
    def destroy(self) -> None:
        self._map.destroyActor(self)

    @ExecSplit(success=(True,), fail=(False,))
    @TypeAdapter(offset=([tuple, list], Vector2i))
    def MapMove(self, offset: Union[Vector2i, Pair[int], List[int]]) -> None:
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
                Actor.BlueprintEvent(self, Actor, "onCollision", {"other": collisions})
                for collision in collisions:
                    Actor.BlueprintEvent(collision, Actor, "onCollision", {"other": [self]})
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
        return self.tickable

    @ExecSplit(default=(None,))
    def setTickable(self, tickable: bool, applyToChildren: bool = True) -> None:
        self.tickable = tickable
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
        return self.collisionEnabled

    @ExecSplit(default=(None,))
    def setCollisionEnabled(self, enabled: bool) -> None:
        self.collisionEnabled = enabled

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

        topMaterial = self._map.getTopMaterial(self.getMapPosition())
        speed = self.speed
        if not topMaterial is None:
            speed *= topMaterial.speedRate
        dist = self._destination - self._departure
        length = dist.length()
        time = length / speed
        velocity = dist / time
        return velocity

    @staticmethod
    def GenActor(
        ActorModel: type, textureStr: str, textureRect: Optional[Tuple[Pair[int], Pair[int]]], tag: str
    ) -> Actor:
        from Engine import Manager

        actor: Actor = ActorModel(Manager.loadCharacter(textureStr), textureRect, tag)
        if isinstance(actor.material, dict):
            actor.material = Material(**Inner.filterDataClassParams(actor.material, Material))
        return actor

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
                Actor.BlueprintEvent(self, Actor, "onOverlap", {"other": overlaps})
                for overlap in overlaps:
                    Actor.BlueprintEvent(overlap, Actor, "onOverlap", {"other": [self]})

    def _autoFixMapPosition(self) -> None:
        pos = self.getPosition()
        x = int(pos.x * 1.0 / GetCellSize() + 0.5)
        y = int(pos.y * 1.0 / GetCellSize() + 0.5)
        self.setMapPosition(Vector2u(x, y))
        if self._map:
            self._map.markPassabilityDirty()
