# -*- encoding: utf-8 -*-

import warnings
from concurrent.futures import ThreadPoolExecutor, as_completed
from typing import List, Tuple, Union, Optional
from . import Sprite, IntRect, Vector2i, RenderTexture, Vector2f, Angle, degrees, Time, seconds, Utils


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
        size = Utils.Math.ToVector2u(rect.size)
        self.rect: IntRect = rect
        self._canvas: RenderTexture = RenderTexture(size)
        self._parent: Sprite = None
        self._childrenList: List[Sprite] = []
        self._relativePosition: Vector2f = Vector2f(0, 0)
        self._relativeRotation: Angle = Angle(0)
        self._relativeScale: Vector2f = Vector2f(1, 1)
        self._visible: bool = True
        super().__init__(self._canvas.getTexture(), rect)
        self.setPosition(rect.position)

    def v_getPosition(self) -> Tuple[float, float]:
        result = super().getPosition()
        return (result.x, result.y)

    def getRelativePosition(self) -> Vector2f:
        return self._relativePosition

    def v_getRelativePosition(self) -> Tuple[float, float]:
        return (self._relativePosition.x, self._relativePosition.y)

    def setPosition(self, position: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(position, Vector2f):
            if not isinstance(position, tuple):
                raise TypeError("position must be a tuple or Vector2f")
            x, y = position
            position = Vector2f(x, y)
        if self.getParent():
            parentPosition = self.getParent().getPosition()
            self._relativePosition = position - parentPosition
        else:
            self._relativePosition = Vector2f(0, 0)
        super().setPosition(position)

    def move(self, offset: Union[Vector2f, Tuple[float, float]]) -> bool:
        if not isinstance(offset, Vector2f):
            if not isinstance(offset, tuple):
                raise TypeError("offset must be a tuple or Vector2f")
            x, y = offset
            offset = Vector2f(x, y)
        super().move(offset)
        self._relativePosition += offset

    def setRelativePosition(self, position: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(position, Vector2f):
            if not isinstance(position, tuple):
                raise TypeError("position must be a tuple or Vector2f")
            x, y = position
            position = Vector2f(x, y)
        parentPosition = Vector2f(0, 0)
        if self.getParent():
            parentPosition = self.getParent().getPosition()
        self.setPosition(parentPosition + position)

    def v_getRotation(self) -> float:
        result = super().getRotation()
        return result.asDegrees()

    def getRelativeRotation(self) -> Angle:
        return self._relativeRotation

    def v_getRelativeRotation(self) -> float:
        return self._relativeRotation.asDegrees()

    def setRotation(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        if self.getParent():
            parentRotation = self.getParent().getRotation()
            self._relativeRotation = angle - parentRotation
        else:
            self._relativeRotation = degrees(0)
        super().setRotation(angle)

    def rotate(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        self._relativeRotation += angle
        super().rotate(angle)

    def setRelativeRotation(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        parentRotation = degrees(0)
        if self.getParent():
            parentRotation = self.getParent().getRotation()
        self.setRotation(parentRotation + angle)

    def v_getScale(self) -> Tuple[float, float]:
        result = super().getScale()
        return (result.x, result.y)

    def getRelativeScale(self) -> Vector2f:
        return self._relativeScale

    def v_getRelativeScale(self) -> Tuple[float, float]:
        return (self._relativeScale.x, self._relativeScale.y)

    def setScale(self, scale: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(scale, Vector2f):
            if not isinstance(scale, tuple):
                raise TypeError("scale must be a tuple or Vector2f")
            x, y = scale
            scale = Vector2f(x, y)
        if self.getParent():
            parentScale = self.getParent().getScale()
            self._relativeScale = scale.componentWiseDiv(parentScale)
        else:
            self._relativeScale = Vector2f(1, 1)
        super().setScale(scale)

    def scale(self, factor: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(factor, Vector2f):
            if not isinstance(factor, tuple):
                raise TypeError("factor must be a tuple or Vector2f")
            x, y = factor
            factor = Vector2f(x, y)

        self._relativeScale = self._relativeScale.componentWiseMul(factor)
        super().scale(factor)

    def setRelativeScale(self, scale: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(scale, Vector2f):
            if not isinstance(scale, tuple):
                raise TypeError("scale must be a tuple or Vector2f")
            x, y = scale
            scale = Vector2f(x, y)
        parentScale = Vector2f(1, 1)
        if self.getParent():
            parentScale = self.getParent().getScale()
        self.setScale(parentScale.componentWiseMul(scale))

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

    def getParent(self) -> Optional[Sprite]:
        return self._parent

    def setParent(self, parent: Optional[Sprite]) -> None:
        self._parent = parent

    def getChildren(self) -> List[Sprite]:
        return self._childrenList

    def addChild(self, child) -> None:
        if not type(child) == Sprite:
            warnings.warn("child must be a Sprite")
            return
        self._childrenList.append(child)

    def removeChild(self, child: Sprite) -> None:
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
        self._canvas.clear()
        logicalFuture = ThreadPoolExecutor().submit(self._logicHandle, deltaTime)
        for future in as_completed([logicalFuture]):
            try:
                future.result()
            except Exception as e:
                print(e)

    def _logicHandle(self, deltaTime: float) -> None:
        pass

    def _renderHandle(self, deltaTime: float) -> None:
        for child in self._childrenList:
            self._canvas.draw(child)
        self._canvas.display()
