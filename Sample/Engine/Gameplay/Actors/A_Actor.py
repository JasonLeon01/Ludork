# -*- encoding: utf-8 -*-

from __future__ import annotations
from . import A_Base
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from . import Vector2f

if TYPE_CHECKING:
    from Engine import Texture, IntRect


class Actor(A_Base.Actor):

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]] = None,
        tag: str = "",
    ) -> None:

        super().__init__(texture, rect, tag)
        self._collisionEnabled: bool = False
        self._active: bool = True
        self._tickable: bool = False

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

    def onOverlapStart(self, other: List[Actor]) -> None:
        pass

    def onOverlapEnd(self, other: List[Actor]) -> None:
        pass

    def isActive(self) -> bool:
        return self._active

    def destroy(self) -> None:
        self._active = False
        self._scene.destroyActor(self)

    def move(self, offset: Union[Vector2f, Tuple[float, float]]) -> bool:
        assert isinstance(offset, (Vector2f, tuple)), "offset must be a tuple or Vector2f"
        if not isinstance(offset, Vector2f):
            x, y = offset
            offset = Vector2f(x, y)
        self._superMove(offset)
        collideActors = self._scene.getCollision(self)
        if collideActors:
            self._superMove(-offset)
            if self.getParent():
                parentPosition = self.getParent().getPosition()
                self._relativePosition = self.getPosition() - parentPosition
            self.onCollision(collideActors)
            for actor in collideActors:
                actor.onCollision([self])
            return False
        self._relativePosition += offset
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()
        return True

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
