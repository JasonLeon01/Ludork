# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from . import Texture, IntRect, Vector2i, Utils
from . import A_Actor


class Actor(A_Actor.Actor):

    def __init__(self, texture: Optional[Texture] = None, tag: str = "") -> None:
        if not texture is None and not isinstance(texture, Texture):
            raise TypeError("texture must be a Texture")
        self._rectSize: Vector2i = Utils.Math.ToVector2i(texture.getSize() / 4)
        rect = IntRect(Vector2i(0, 0), self._rectSize)
        super().__init__(texture, rect, tag)
        self.direction: int = 0
        self.directionFix: bool = False
        self.animateWithoutMoving: bool = False
        self._sx: int = 0
        self._sy: int = 0

    def setSpriteTexture(self, texture: Texture, resetRect: bool = False):
        super().setSpriteTexture(texture, resetRect)
        if resetRect:
            sx_i = self._sx // self._rectSize.x
            sy_i = self._sy // self._rectSize.y
            self._rectSize = Utils.Math.ToVector2i(texture.getSize() / 4)
            self.setTextureRect(IntRect(Vector2i(sx_i * self._rectSize.x, sy_i * self._rectSize.y), self._rectSize))

    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        if not isinstance(texture, Texture):
            raise TypeError("texture must be a Texture")
        super().setTexture(texture, resetRect)

    def setTextureRect(self, rectangle: IntRect):
        self._rectSize = rectangle.size
        return super().setTextureRect(rectangle)

    def update(self, deltaTime: float):
        if not self.directionFix:
            velocity = self.getVelocity()
            if velocity:
                vx = velocity.x
                vy = velocity.y
                if abs(vx) > abs(vy):
                    self.direction = 2 if vx > 0 else 1
                else:
                    self.direction = 3 if vy > 0 else 0
            self._sy = self.direction * self._rectSize.y
            if self._children:
                print(f"{self._sy} {self.direction} {self._rectSize.y}")
        super().update(deltaTime)

    def _animate(self, deltaTime: float):
        if self.isMoving() or self.animateWithoutMoving:
            self._switchTimer += deltaTime
            if self._switchTimer >= self.switchInterval:
                self._switchTimer = 0.0
                self._sx = (self._sx + self._rectSize.x) % self.getSpriteTexture().getSize().x
        else:
            self._sx = 0
        self.setTextureRect(IntRect(Vector2i(self._sx, self._sy), self._rectSize))
