# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import TypeAlias, Callable, List, Optional, Tuple, Union, TYPE_CHECKING
from . import Vector2f
from . import A_Actor

if TYPE_CHECKING:
    from Engine import Texture, IntRect


VelocityProcessor: TypeAlias = Callable[[float], Vector2f]


class Actor(A_Actor.Actor):

    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]] = None,
        tag: str = "",
    ) -> None:
        super().__init__(texture, rect, tag)
        self.velocity: Vector2f = Vector2f(0, 0)
        self.velocityProcessor: Optional[VelocityProcessor] = None
        self.expireTime: Optional[float] = None
        self._flyTimer: float = 0.0

    def getVelocity(self) -> Optional[Vector2f]:
        v = super().getVelocity()
        if v is None:
            return self.velocity
        return v + self.velocity

    def update(self, deltaTime: float) -> None:
        if not self.expireTime is None:
            self.expireTime -= deltaTime
            if self.expireTime <= 0:
                if self.isActive():
                    self.destroy()
                return
        self._flyTimer += deltaTime
        if not self.velocityProcessor is None:
            self.velocity = self.velocityProcessor(self._flyTimer)
        return super().update(deltaTime)

    @staticmethod
    def Create(texture: Optional[Union[Texture, List[Texture]]] = None):
        return Actor(texture)

    def _autoMove(self, deltaTime: float) -> None:
        zeroStart = False
        if self._velocity is None:
            zeroStart = True
            self._velocity = Vector2f(0, 0)
        self._velocity = self._velocity + self.velocity
        super()._autoMove(deltaTime)
        self._velocity = self._velocity - self.velocity
        if zeroStart:
            self._velocity = None
