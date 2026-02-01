# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, Tuple, List
from ... import Pair, Texture, IntRect, Vector2i, Utils, ExecSplit, InvalidVars
from ..G_Material import Material
from .A_Actor import Actor


@InvalidVars("defaultRect")
class Character(Actor):
    direction: int = 0
    directionFix: bool = False
    animateWithoutMoving: bool = False

    def __init__(self, texture: Optional[Texture] = None, tag: Optional[str] = None) -> None:
        if not texture is None:
            assert isinstance(texture, Texture), "texture must be a Texture"
        self._rectSize: Vector2i = Utils.Math.ToVector2i(texture.getSize() / 4)
        rect = IntRect(Vector2i(0, 0), self._rectSize)
        super().__init__(texture, rect, tag)
        self._sx: int = 0
        self._sy: int = 0

    @ExecSplit(default=(None,))
    def setSpriteTexture(self, texture: Texture, resetRect: bool = False) -> None:
        super().setSpriteTexture(texture, resetRect)
        if resetRect:
            sx_i = self._sx // self._rectSize.x
            sy_i = self._sy // self._rectSize.y
            self._rectSize = Utils.Math.ToVector2i(texture.getSize() / 4)
            self.setTextureRect(IntRect(Vector2i(sx_i * self._rectSize.x, sy_i * self._rectSize.y), self._rectSize))

    @ExecSplit(default=(None,))
    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        assert isinstance(texture, Texture), "texture must be a Texture"
        super().setTexture(texture, resetRect)

    @ExecSplit(default=(None,))
    def setTextureRect(self, rectangle: IntRect) -> None:
        self._rectSize = rectangle.size
        return super().setTextureRect(rectangle)

    @ExecSplit(success=(True,), fail=(False,))
    def MapMove(self, offset: Union[Vector2i, Pair[int], List[int]]) -> None:
        result = super().MapMove(offset)
        if not result:
            assert isinstance(offset, (Vector2i, Tuple, List)), "offset must be a Vector2i or a tuple"
            if isinstance(offset, (tuple, list)):
                offset = Vector2i(*offset)
                vx = offset.x
                vy = offset.y
                self._applyDirection(vx, vy)
        return result

    def update(self, deltaTime: float) -> None:
        if not self.directionFix:
            velocity = self.getVelocity()
            if velocity:
                vx = velocity.x
                vy = velocity.y
                self._applyDirection(vx, vy)
            self._sy = self.direction * self._rectSize.y
        super().update(deltaTime)

    @staticmethod
    def GenActor(
        ActorModel: type, textureStr: str, textureRect: Optional[Tuple[Pair[int], Pair[int]]], tag: str
    ) -> Character:
        from Engine import Manager

        character: Character = ActorModel(Manager.loadCharacter(textureStr), tag)
        if isinstance(character.material, dict):
            character.material = Material(**character.material)
        return character

    def _animate(self, deltaTime: float) -> None:
        if self.isMoving() or self.animateWithoutMoving:
            self._switchTimer += deltaTime
            if self._switchTimer >= self.switchInterval:
                self._switchTimer = 0.0
                self._sx = (self._sx + self._rectSize.x) % self.getSpriteTexture().getSize().x
        else:
            self._sx = 0
        self.setTextureRect(IntRect(Vector2i(self._sx, self._sy), self._rectSize))

    def _applyDirection(self, vx: float, vy: float) -> None:
        if abs(vx) > abs(vy):
            self.direction = 2 if vx > 0 else 1
        else:
            self.direction = 0 if vy > 0 else 3
