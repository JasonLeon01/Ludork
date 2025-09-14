# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
import warnings
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from . import Sprite, Texture, IntRect, Vector2i, Vector2f, Angle, degrees, Time, GetCellSize, Utils

if TYPE_CHECKING:
    from Engine.Gameplay.Scenes import Base


class Actor(Sprite):
    def __init__(
        self,
        texture: Optional[Union[Texture, List[Texture]]] = None,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]] = None,
        tag: str = "",
    ) -> None:
        if not rect is None:
            if not isinstance(rect, IntRect):
                if not isinstance(rect, tuple) or len(rect) != 2:
                    raise TypeError("rect must be a tuple or IntRect")
                position, size = rect
                x, y = position
                w, h = size
                position = Vector2i(x, y)
                size = Vector2i(w, h)
                rect = IntRect(position, size)
        args = []
        if isinstance(texture, list):
            args.append(texture[0])
        else:
            args.append(texture)
        if not rect is None:
            args.append(rect)
        super().__init__(*args)

        self.switchInterval: float = 0.4
        self.tag: str = tag

        self._scene: Base = None
        self._parent: Optional[Actor] = None
        self._children: List[Actor] = []
        self._visible: bool = True
        self._animatable: bool = False
        self._texture: Optional[Union[List[Texture], Texture]] = texture
        self._textureIndex: int = 0
        self._switchTimer: float = 0.0
        self._relativePosition: Vector2f = Vector2f(0, 0)
        self._relativeRotation: Angle = degrees(0)
        self._relativeScale: Vector2f = Vector2f(1, 1)
        self._moveSet: List[Tuple[Vector2f, Time]] = []
        self._velocity: Optional[Vector2f] = None
        self._departure: Optional[Vector2f] = None
        self._destination: Optional[Vector2f] = None

    def update(self, deltaTime: float) -> None:
        if self._moveSet:
            if self._velocity is None:
                self._departure = self.getPosition()
                self._destination, time = self._moveSet[0]
                self._velocity = (self._destination - self._departure) / time.asSeconds()
        self._autoMove(deltaTime)
        if self._animatable:
            self._animate(deltaTime)

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
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()

    def move(self, offset: Union[Vector2f, Tuple[float, float]]) -> bool:
        if not isinstance(offset, Vector2f):
            if not isinstance(offset, tuple):
                raise TypeError("offset must be a tuple or Vector2f")
            x, y = offset
            offset = Vector2f(x, y)
        super().move(offset)
        self._relativePosition += offset
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()
        return True

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
        if self.getChildren():
            for child in self.getChildren():
                child._updateRotationFromParent()

    def rotate(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        self._relativeRotation += angle
        super().rotate(angle)
        if self.getChildren():
            for child in self.getChildren():
                child._updateRotationFromParent()

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
        if self.getChildren():
            for child in self.getChildren():
                child._updateScaleFromParent()

    def scale(self, factor: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(factor, Vector2f):
            if not isinstance(factor, tuple):
                raise TypeError("factor must be a tuple or Vector2f")
            x, y = factor
            factor = Vector2f(x, y)

        self._relativeScale = self._relativeScale.componentWiseMul(factor)
        super().scale(factor)
        if self.getChildren():
            for child in self.getChildren():
                child._updateScaleFromParent()

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

    def setAlignment(self, alignment: Tuple[float, float]) -> None:
        x, y = alignment
        x = Utils.Math.Clamp(x, 0, 1)
        y = Utils.Math.Clamp(y, 0, 1)
        size = self.getTextureRect().size
        origin = Vector2f(size.x * x, size.y * y)
        self.setOrigin(origin)

    def getScene(self) -> Base:
        return self._scene

    def setScene(self, scene: Base) -> None:
        self._scene = scene

    def getParent(self) -> Optional[Actor]:
        return self._parent

    def setParent(self, parent: Optional[Actor]) -> None:
        self._parent = parent

    def getChildren(self) -> List[Actor]:
        return self._children

    def addChild(self, child: Actor) -> None:
        if child in self._children:
            warnings.warn("Child already exists")
            return
        self._children.append(child)
        child.setParent(self)
        if self._scene:
            child.setScene(self._scene)
            self._scene.updateActorList()

    def removeChild(self, child: Actor) -> None:
        if child not in self._children:
            raise ValueError("Child not found")
        self._children.remove(child)

    def getVisible(self) -> bool:
        return self._visible

    def setVisible(self, visible: bool, applyToChildren: bool = True) -> None:
        self._visible = visible
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setVisible(visible, applyToChildren)

    def getAnimatable(self) -> bool:
        return self._animatable

    def setAnimatable(self, animate: bool, applyToChildren: bool = True) -> None:
        self._animatable = animate
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setAnimatable(animate, applyToChildren)

    def getSpriteTexture(self) -> Optional[Texture]:
        return super().getTexture()

    def getTexture(self) -> Optional[Union[List[Texture], Texture]]:
        return self._texture

    def setSpriteTexture(self, texture: Texture, resetRect: bool = False) -> None:
        super().setTexture(texture, resetRect)

    def setTexture(self, texture: Union[Texture, List[Texture]], resetRect: bool = False) -> None:
        self._texture = texture
        targetTexture = texture
        if isinstance(texture, list):
            targetTexture = texture[0]
        self.setSpriteTexture(targetTexture, resetRect)

    def getMoveSet(self) -> List[Tuple[Vector2f, Time]]:
        return copy.copy(self._moveSet)

    def setMoveSet(self, moveSet: List[Tuple[Vector2f, Time]]) -> None:
        self._moveSet = moveSet

    def isMoving(self) -> bool:
        return len(self._moveSet) > 0

    def getVelocity(self) -> Optional[Vector2f]:
        return self._velocity

    def _superMove(self, offset: Union[Vector2f, Tuple[float, float]]) -> None:
        if not isinstance(offset, Vector2f):
            if not isinstance(offset, tuple):
                raise TypeError("offset must be a tuple or Vector2f")
            x, y = offset
            offset = Vector2f(x, y)
        super().move(offset)

    def _updatePositionFromParent(self) -> None:
        if self.getParent():
            parentPosition = self.getParent().getPosition()
            newPosition = parentPosition + self._relativePosition
            super().setPosition(newPosition)
            if self.getChildren():
                for child in self.getChildren():
                    child._updatePositionFromParent()

    def _updateRotationFromParent(self) -> None:
        if self.getParent():
            parentRotation = self.getParent().getRotation()
            newRotation = parentRotation + self._relativeRotation
            super().setRotation(newRotation)
            if self.getChildren():
                for child in self.getChildren():
                    child._updateRotationFromParent()

    def _updateScaleFromParent(self) -> None:
        if self.getParent():
            parentScale = self.getParent().getScale()
            newScale = parentScale.componentWiseMul(self._relativeScale)
            super().setScale(newScale)
            if self.getChildren():
                for child in self.getChildren():
                    child._updateScaleFromParent()

    def _autoMove(self, deltaTime: float) -> None:
        if not self._velocity is None:
            if not self.move(self._velocity * deltaTime):
                self._moveSet.clear()
                self._departure = None
                self._destination = None
                self._velocity = None
                warnings.warn("Actor.move() failed, moveSet cleared")
            elif not self._destination is None:
                if Utils.Math.HasReachedDestination(self._departure, self._destination, self.getPosition()):
                    self._moveSet.pop(0)
                    self.setPosition(self._destination)
                    self._departure = None
                    self._destination = None
                    self._velocity = None
                else:
                    return
            else:
                return
            intBasedPosition = Utils.Math.Vector2fRound(self.getPosition())
            cellBasedX = int(intBasedPosition.x / GetCellSize()) * GetCellSize()
            cellBasedY = int(intBasedPosition.y / GetCellSize()) * GetCellSize()
            self.setPosition(Vector2f(cellBasedX, cellBasedY))

    def _animate(self, deltaTime: float) -> None:
        if isinstance(self._texture, list):
            self._switchTimer += deltaTime
            if self._switchTimer >= self.switchInterval:
                self._textureIndex = (self._textureIndex + 1) % len(self._texture)
                self.setSpriteTexture(self._texture[self._textureIndex])
                self._switchTimer = 0.0
