# -*- encoding: utf-8 -*-

from typing import Optional, Tuple, Union
from . import (
    Sprite,
    Texture,
    IntRect,
    Vector2f,
    Angle,
    degrees,
)


class UI(Sprite):
    def __init__(self, texture: Texture, rect: Optional[IntRect] = None) -> None:
        self._visible: bool = True
        if rect:
            super().__init__(texture, rect)
        else:
            super().__init__(texture)

    def v_getPosition(self) -> Tuple[float, float]:
        result = super().getPosition()
        return (result.x, result.y)

    def setPosition(self, position: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(position, Vector2f):
            if not isinstance(position, tuple):
                raise TypeError("position must be a tuple or Vector2f")
            x, y = position
            position = Vector2f(x, y)
        super().setPosition(position)

    def move(self, offset: Union[Vector2f, Tuple[float, float]]) -> bool:
        if not isinstance(offset, Vector2f):
            if not isinstance(offset, tuple):
                raise TypeError("offset must be a tuple or Vector2f")
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
        if not isinstance(scale, Vector2f):
            if not isinstance(scale, tuple):
                raise TypeError("scale must be a tuple or Vector2f")
            x, y = scale
            scale = Vector2f(x, y)
        super().setScale(scale)

    def scale(self, factor: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(factor, Vector2f):
            if not isinstance(factor, tuple):
                raise TypeError("factor must be a tuple or Vector2f")
            x, y = factor
            factor = Vector2f(x, y)
        super().scale(factor)

    def v_getOrigin(self) -> Tuple[float, float]:
        result = super().getOrigin()
        return (result.x, result.y)

    def setOrigin(self, origin: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(origin, Vector2f):
            if not isinstance(origin, tuple):
                raise TypeError("origin must be a tuple or Vector2f")
            x, y = origin
            origin = Vector2f(x, y)
        return super().setOrigin(origin)

    def getVisible(self) -> bool:
        return self._visible

    def setVisible(self, visible: bool) -> None:
        self._visible = visible
