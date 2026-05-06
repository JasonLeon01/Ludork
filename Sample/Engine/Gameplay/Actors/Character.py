# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, Tuple, List
from ... import Direction, Pair, Texture, IntRect, Vector2i, Utils
from ...Utils import Inner
from ..Material import Material
from .Actor import Actor


@InvalidVars("defaultRect")
class Character(Actor):
    """Actor subclass with directional sprite-sheet animation.

    Automatically selects the correct row/column of a 4-direction
    character sprite-sheet based on movement velocity.
    Texture is expected to be a 4x4 grid (columns = frames, rows = directions).
    """

    direction: Direction = Direction.DOWN  #: Current facing direction
    directionFix: bool = False             #: If `True`, direction never updates from movement
    animateWithoutMoving: bool = False     #: If `True`, animation plays even when idle

    def __init__(self, texture: Texture, tag: Optional[str] = None) -> None:
        """Construct a character from a 4-direction sprite-sheet texture."""
        if not texture is None:
            assert isinstance(texture, Texture), "texture must be a Texture"
        self._rectSize: Vector2i = Utils.Math.ToVector2i(texture.getSize() / 4)
        rect = IntRect(Vector2i(0, 0), self._rectSize)
        super().__init__(texture, rect, tag)
        self._sx: int = 0
        self._sy: int = 0

    @ExecSplit(default=(None,))
    def setSpriteTexture(self, texture: Texture, resetRect: bool = False) -> None:
        """Set the sprite texture, optionally resetting the rect to the new sheet size."""
        super().setSpriteTexture(texture, resetRect)
        if resetRect:
            sx_i = self._sx // self._rectSize.x
            sy_i = self._sy // self._rectSize.y
            self._rectSize = Utils.Math.ToVector2i(texture.getSize() / 4)
            self.setTextureRect(IntRect(Vector2i(sx_i * self._rectSize.x, sy_i * self._rectSize.y), self._rectSize))

    @ExecSplit(default=(None,))
    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        """Set the character texture (must be a valid Texture instance)."""
        assert isinstance(texture, Texture), "texture must be a Texture"
        super().setTexture(texture, resetRect)

    @ExecSplit(default=(None,))
    def setTextureRect(self, rectangle: IntRect) -> None:
        """Set the texture sub-rectangle and update internal rect size."""
        self._rectSize = rectangle.size
        return super().setTextureRect(rectangle)

    @ExecSplit(success=(True,), fail=(False,))
    @TypeAdapter(offset=([tuple, list], Vector2i))
    def MapMove(self, offset: Union[Vector2i, Pair[int], List[int]]) -> bool:
        """Move one grid cell and update facing direction on failure."""
        if not self._moveEnabled:
            return False
        result = super().MapMove(offset)
        if not result:
            vx = offset.x
            vy = offset.y
            self._applyDirection(vx, vy)
        return result

    def update(self, deltaTime: float) -> None:
        """Update facing direction from velocity and advance animation."""
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
        ActorModel: type, texture: Texture, textureRect: Optional[Tuple[Pair[int], Pair[int]]], tag: str
    ) -> Character:
        """Factory: instantiate a character from a blueprint-generated class model."""
        character: Character = ActorModel(texture, tag)
        if isinstance(character.material, dict):
            character.material = Material(**Inner.filterDataClassParams(character.material, Material))
        return character

    def _animate(self, deltaTime: float) -> None:
        spriteTexture = self.getSpriteTexture()
        if spriteTexture is None:
            return
        if self.isMoving() or self.animateWithoutMoving:
            self._switchTimer += deltaTime
            if self._switchTimer >= self.switchInterval:
                self._switchTimer = 0.0
                self._sx = (self._sx + self._rectSize.x) % spriteTexture.getSize().x
        else:
            self._sx = 0
        self.setTextureRect(IntRect(Vector2i(self._sx, self._sy), self._rectSize))

    def _applyDirection(self, vx: float, vy: float) -> None:
        if abs(vx) > abs(vy):
            self.direction = Direction.RIGHT if vx > 0 else Direction.LEFT
        else:
            self.direction = Direction.DOWN if vy > 0 else Direction.UP
