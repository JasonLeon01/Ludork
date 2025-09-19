# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import List, Tuple, Union, Optional
from . import (
    Sprite,
    Drawable,
    IntRect,
    Vector2i,
    Vector2u,
    Vector2f,
    RenderTexture,
    View,
    FloatRect,
    Angle,
    degrees,
    Color,
    Utils,
)


class UI(Sprite):
    def __init__(self, rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]]) -> None:
        if not isinstance(rect, IntRect):
            if not isinstance(rect, tuple) or len(rect) != 2:
                raise TypeError("rect must be a tuple or IntRect")
            position, size = rect
            x, y = position
            w, h = size
            position = Vector2i(x, y)
            size = Vector2i(w, h)
            rect = IntRect(position, size)
        self._size = Utils.Math.ToVector2u(rect.size)
        size = Utils.Math.ToVector2u(self._getRealSize(rect.size))
        self._canvas: RenderTexture = RenderTexture(size)
        self._internalView = View(FloatRect(0, 0, self._size.x, self._size.y))
        self._parent: Optional[UI] = None
        self._childrenList: List[Drawable] = []
        self._visible: bool = True
        super().__init__(self._canvas.getTexture())
        self.setPosition(rect.position)

    def getSize(self) -> Vector2u:
        return self._size

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
        super().move(offset)

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

    def getParent(self) -> Optional[UI]:
        return self._parent

    def setParent(self, parent: Optional[UI]) -> None:
        self._parent = parent

    def getChildren(self) -> List[Drawable]:
        return self._childrenList

    def addChild(self, child) -> None:
        from Engine.Gameplay import Actor

        assert not isinstance(child, Actor), "Cannot add Actor to UI"
        self._childrenList.append(child)
        if isinstance(child, UI):
            child.setParent(self)

    def removeChild(self, child: Drawable) -> None:
        if child not in self._childrenList:
            raise ValueError("Child not found")
        self._childrenList.remove(child)

    def getVisible(self) -> bool:
        return self._visible

    def setVisible(self, visible: bool) -> None:
        self._visible = visible

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def update(self, deltaTime: float) -> None:
        if not self._visible:
            return
        for child in self._childrenList:
            if isinstance(child, UI):
                child.update(deltaTime)
        self.onTick(deltaTime)
        self._canvas.clear(Color.Transparent)
        self._canvas.setView(self._internalView)
        for child in self._childrenList:
            if hasattr(child, "getVisible"):
                if not child.getVisible():
                    continue
            self._canvas.draw(child)
        self._canvas.setView(self._canvas.getDefaultView())
        self._canvas.display()
        self.onLateTick(deltaTime)

    def _getScale(self) -> float:
        from Engine import System

        return System.getScale()

    def _getRealSize(self, inSize: Union[Vector2i, Vector2u, Vector2f]):
        if not isinstance(inSize, Vector2i) and not isinstance(inSize, Vector2u):
            assert isinstance(inSize, Vector2f), "inSize must be a Vector2i, Vector2u or Vector2f"
            size = copy.copy(inSize)
        else:
            size = Utils.Math.ToVector2f(inSize)
        return size * self._getScale()
