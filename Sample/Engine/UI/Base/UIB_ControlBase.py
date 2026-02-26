# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Tuple, List, Union, TYPE_CHECKING
from ... import TypeAdapter, Pair, Drawable, Transformable, Vector2f, Angle, degrees, Transform

if TYPE_CHECKING:
    from Engine.UI import Canvas, ListView


class ControlBase(Drawable, Transformable):
    def __init__(self) -> None:
        Drawable.__init__(self)
        Transformable.__init__(self)
        self._visible: bool = True
        self._active: bool = True
        self._name: str = ""
        self._parent: Optional[Union[Canvas, ListView]] = None

    def getVisible(self) -> bool:
        return self._visible

    def setVisible(self, visible: bool) -> None:
        self._visible = visible

    def getActive(self) -> bool:
        return self._active

    def setActive(self, active: bool) -> None:
        self._active = active

    def getName(self) -> str:
        return self._name

    def setName(self, name: str) -> None:
        self._name = name

    def getParent(self) -> Optional[Union[Canvas, ListView]]:
        return self._parent

    def setParent(self, parent: Optional[Union[Canvas, ListView]]) -> None:
        self._parent = parent

    def v_getPosition(self) -> Pair[float]:
        result = super().getPosition()
        return (result.x, result.y)

    @TypeAdapter(position=([tuple, list], Vector2f))
    def setPosition(self, position: Union[Vector2f, Pair[float], List[float]]) -> None:
        super().setPosition(position)

    @TypeAdapter(offset=([tuple, list], Vector2f))
    def move(self, offset: Union[Vector2f, Pair[float], List[float]]) -> bool:
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

    def v_getScale(self) -> Pair[float]:
        result = super().getScale()
        return (result.x, result.y)

    @TypeAdapter(scale=([tuple, list], Vector2f))
    def setScale(self, scale: Union[Vector2f, Pair[float], List[float]]) -> None:
        super().setScale(scale)

    @TypeAdapter(factor=([tuple, list], Vector2f))
    def scale(self, factor: Union[Vector2f, Pair[float], List[float]]) -> None:
        super().scale(factor)

    def v_getOrigin(self) -> Pair[float]:
        result = super().getOrigin()
        return (result.x, result.y)

    @TypeAdapter(origin=([tuple, list], Vector2f))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        return super().setOrigin(origin)

    def _getScreenTransform(self) -> Transform:
        transform = self.getTransform()
        if self._parent:
            return self._parent._getScreenTransform() * transform
        return transform

    def _getRenderTransform(self) -> Transform:
        return self.getTransform()

    def _getScreenRenderTransform(self) -> Transform:
        transform = self._getRenderTransform()
        if self._parent:
            return self._parent._getScreenRenderTransform() * transform
        return transform
