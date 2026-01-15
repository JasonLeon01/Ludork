# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Tuple, Union, TYPE_CHECKING
from .. import (
    Drawable,
    Transformable,
    Sprite,
    Vector2f,
    Angle,
    degrees,
)
from .UI_ControlBase import ControlBase

if TYPE_CHECKING:
    from Engine import Texture, IntRect, RenderStates, Color, FloatRect, RenderTexture


class SpriteBase(Transformable, Drawable, ControlBase):
    def __init__(self, texture: Texture, rect: Optional[IntRect] = None) -> None:
        from ..Utils import Render

        self._sprite: Sprite
        self._renderStates: RenderStates = Render.CanvasRenderStates()
        if rect:
            self._sprite = Sprite(texture, rect)
        else:
            self._sprite = Sprite(texture)
        Transformable.__init__(self)
        Drawable.__init__(self)
        ControlBase.__init__(self)

    def v_getPosition(self) -> Tuple[float, float]:
        result = super().getPosition()
        return (result.x, result.y)

    def setPosition(self, position: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(position, (Vector2f, tuple)), "position must be a tuple or Vector2f"
        if not isinstance(position, Vector2f):
            x, y = position
            position = Vector2f(x, y)
        super().setPosition(position)

    def move(self, offset: Union[Vector2f, Tuple[float, float]]) -> bool:
        assert isinstance(offset, (Vector2f, tuple)), "offset must be a tuple or Vector2f"
        if not isinstance(offset, Vector2f):
            x, y = offset
            offset = Vector2f(x, y)
        return super().move(offset)

    def v_getRotation(self) -> float:
        result = super().getRotation()
        return result.asDegrees()

    def setRotation(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        super().setRotation(angle)

    def rotate(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        super().rotate(angle)

    def v_getScale(self) -> Tuple[float, float]:
        result = super().getScale()
        return (result.x, result.y)

    def setScale(self, scale: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(scale, (Vector2f, tuple)), "scale must be a tuple or Vector2f"
        if not isinstance(scale, Vector2f):
            x, y = scale
            scale = Vector2f(x, y)
        super().setScale(scale)

    def scale(self, factor: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(factor, (Vector2f, tuple)), "factor must be a tuple or Vector2f"
        if not isinstance(factor, Vector2f):
            x, y = factor
            factor = Vector2f(x, y)
        super().scale(factor)

    def v_getOrigin(self) -> Tuple[float, float]:
        result = super().getOrigin()
        return (result.x, result.y)

    def setOrigin(self, origin: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(origin, (Vector2f, tuple)), "origin must be a tuple or Vector2f"
        if not isinstance(origin, Vector2f):
            x, y = origin
            origin = Vector2f(x, y)
        return super().setOrigin(origin)

    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        self._sprite.setTexture(texture, resetRect)

    def getTexture(self) -> Texture:
        return self._sprite.getTexture()

    def setTextureRect(self, rect: IntRect) -> None:
        self._sprite.setTextureRect(rect)

    def getTextureRect(self) -> IntRect:
        return self._sprite.getTextureRect()

    def setColor(self, color: Color) -> None:
        self._sprite.setColor(color)

    def getColor(self) -> Color:
        return self._sprite.getColor()

    def getLocalBounds(self) -> FloatRect:
        return self._sprite.getLocalBounds()

    def getGlobalBounds(self) -> FloatRect:
        return self._sprite.getGlobalBounds()

    def getRenderStates(self) -> RenderStates:
        return self._renderStates

    def draw(self, target: RenderTexture, states: RenderStates) -> None:
        self._applyRenderStates(states)
        target.draw(self._sprite, states)

    def _applyRenderStates(self, states: RenderStates) -> None:
        from .. import System

        states.transform *= self.getTransform()
        states.transform.translate(Vector2f(System.getScale() - 1, System.getScale() - 1))
