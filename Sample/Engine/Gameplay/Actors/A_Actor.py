# -*- encoding: utf-8 -*-

from __future__ import annotations
from . import A_Base
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from . import Vector2f, Vector2i, GetCellSize
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
        self._speed: float = 32.0
        self._isMoving: bool = False
        self._departure: Optional[Vector2f] = None
        self._destination: Optional[Vector2f] = None

    def update(self, deltaTime: float) -> None:
        if self._isMoving:
            self._processMoving(deltaTime)
        super().update(deltaTime)

    def onCreate(self) -> None:
        pass

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onDestroy(self) -> None:
        pass

    def onCollision(self, other: List[Actor]) -> None:
        pass

    def onOverlap(self, other: List[Actor]) -> None:
        pass

    def destroy(self) -> None:
        self._map.destroyActor(self)

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
            return
        if self._isMoving:
            return
        target = self.getMapPosition() + offset
        if target.x < 0 or target.x >= self._map.getSize().x or target.y < 0 or target.y >= self._map.getSize().y:
            return
        if not self._map.isPassable(self, target):
            collisions = self._map.getCollision(self, target)
            if collisions:
                self.onCollision(collisions)
                for collision in collisions:
                    collision.onCollision([self])
            return
        self._isMoving = True
        self._departure = self.getPosition()
        self._destination = Vector2f(
            self.getPosition().x + offset.x * GetCellSize(),
            self.getPosition().y + offset.y * GetCellSize(),
        )

    def getTickable(self) -> bool:
        return self._tickable

    def setTickable(self, tickable: bool, applyToChildren: bool = True) -> None:
        self._tickable = tickable
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    if isinstance(child, Actor):
                        child.setTickable(tickable, applyToChildren)

    def getVisible(self) -> bool:
        return self._visible

    def setVisible(self, visible: bool, applyToChildren: bool = True) -> None:
        self._visible = visible
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setVisible(visible, applyToChildren)

    def getCollisionEnabled(self) -> bool:
        return self._collisionEnabled

    def setCollisionEnabled(self, enabled: bool) -> None:
        self._collisionEnabled = enabled

    def intersects(self, other: Actor) -> bool:
        return self.getGlobalBounds().findIntersection(other.getGlobalBounds()) != None

    def isMoving(self) -> bool:
        return self._isMoving

    def getVelocity(self) -> Optional[Vector2f]:
        if self._departure is None or self._destination is None:
            return None

        dist = self._destination - self._departure
        length = dist.length()
        time = length / self._speed
        velocity = dist / time
        return velocity

    def _processMoving(self, deltaTime: float) -> None:
        velocity = self.getVelocity()
        if velocity is None:
            return
        offset = velocity * deltaTime
        self.move(offset)
        if (
            Math.IsNearZero(self.getPosition().x - self._destination.x, 0.1)
            and Math.IsNearZero(self.getPosition().y - self._destination.y, 0.1)
        ) or (
            (self._destination.x - self._departure.x) * (self.getPosition().x - self._destination.x) > 0
            or (self._destination.y - self._departure.y) * (self.getPosition().y - self._destination.y) > 0
        ):
            self.setPosition(Vector2f(self._destination.x, self._destination.y))
            self._isMoving = False
            self._departure = None
            self._destination = None
            overlaps = self._map.getOverlaps(self)
            if overlaps:
                self.onOverlap(overlaps)
                for overlap in overlaps:
                    overlap.onOverlap([self])
