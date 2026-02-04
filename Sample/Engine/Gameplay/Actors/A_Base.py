# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
import warnings
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from ... import (
    Pair,
    Sprite,
    IntRect,
    Vector2i,
    Vector2u,
    Vector2f,
    Angle,
    degrees,
    GetCellSize,
    Utils,
    ExecSplit,
    ReturnType,
)
from ..G_Material import Material

if TYPE_CHECKING:
    from Engine import Texture
    from Engine.Gameplay import GameMap
    from Engine.NodeGraph import Graph


class _ActorBase(Sprite):
    tag: str = ""
    switchInterval: float = 0.2
    animatable: bool = False
    material: Material = Material()

    def __init__(
        self,
        texture: Optional[Texture] = None,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]], List[List[int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        if not rect is None:
            assert isinstance(rect, (IntRect, Tuple, List)), "rect must be a tuple, list, or IntRect"
            if isinstance(rect, (tuple, list)):
                position, size = rect
                x, y = position
                w, h = size
                position = Vector2i(x, y)
                size = Vector2i(w, h)
                rect = IntRect(position, size)
        args = [texture]
        if not rect is None:
            args.append(rect)
        super().__init__(*args)

        if not tag is None:
            self.tag = tag
        self._map: GameMap = None
        self._parent: Optional[_ActorBase] = None
        self._children: List[_ActorBase] = []
        self._translation: Vector2f = Vector2f(0, 0)
        self._visible: bool = True
        self._texture: Optional[Texture] = texture
        self._switchTimer: float = 0.0
        self._relativePosition: Vector2f = Vector2f(0, 0)
        self._relativeRotation: Angle = degrees(0)
        self._relativeScale: Vector2f = Vector2f(1, 1)
        self._graph: Optional[Graph] = None

    def update(self, deltaTime: float) -> None:
        if self.animatable:
            self._animate(deltaTime)

    def lateUpdate(self, deltaTime: float) -> None:
        pass

    def fixedUpdate(self, fixedDelta: float) -> None:
        pass

    @ReturnType(pos=Vector2f)
    def getPosition(self) -> Vector2f:
        return super().getPosition() - self._translation

    @ReturnType(pos=Pair[float])
    def v_getPosition(self) -> Pair[float]:
        result = super().getPosition()
        return (result.x, result.y)

    @ReturnType(pos=Vector2f)
    def getRelativePosition(self) -> Vector2f:
        return self._relativePosition

    @ReturnType(pos=Pair[float])
    def v_getRelativePosition(self) -> Pair[float]:
        return (self._relativePosition.x, self._relativePosition.y)

    @ReturnType(pos=Vector2i)
    def getMapPosition(self) -> Vector2i:
        return Vector2i(
            int(self.getPosition().x * 1.0 / GetCellSize() + 0.5),
            int(self.getPosition().y * 1.0 / GetCellSize() + 0.5),
        )

    @ReturnType(pos=Pair[int])
    def v_getMapPosition(self) -> Pair[int]:
        return (self.getMapPosition().x, self.getMapPosition().y)

    @ReturnType(pos=Vector2i)
    def getRelativeMapPosition(self) -> Vector2i:
        return Vector2i(
            int(self.getRelativePosition().x / GetCellSize()),
            int(self.getRelativePosition().y / GetCellSize()),
        )

    @ReturnType(pos=Pair[int])
    def v_getRelativeMapPosition(self) -> Pair[int]:
        return (self.getRelativeMapPosition().x, self.getRelativeMapPosition().y)

    @ExecSplit(default=(None,))
    def setPosition(self, position: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(position, (Vector2f, Tuple, List)), "position must be a tuple, list, or Vector2f"
        if isinstance(position, (tuple, list)):
            x, y = position
            position = Vector2f(x, y)
        if self.getParent():
            parentPosition = self.getParent().getPosition()
            self._relativePosition = position - parentPosition
        else:
            self._relativePosition = Vector2f(0, 0)
        super().setPosition(position + self._translation)
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()

    @ExecSplit(default=(None,))
    def setRelativePosition(self, position: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(position, (Vector2f, Tuple, List)), "position must be a tuple, list, or Vector2f"
        if isinstance(position, (tuple, list)):
            x, y = position
            position = Vector2f(x, y)
        parentPosition = Vector2f(0, 0)
        if self.getParent():
            parentPosition = self.getParent().getPosition()
        self.setPosition(parentPosition + position)

    @ExecSplit(default=(None,))
    def setMapPosition(self, position: Union[Vector2u, Pair[int], List[int]]) -> None:
        assert isinstance(position, (Vector2u, Tuple, List)), "position must be a tuple, list, or Vector2u"
        if isinstance(position, (tuple, list)):
            x, y = position
            position = Vector2u(x, y)
        self.setPosition(Vector2f(position.x * GetCellSize(), position.y * GetCellSize()))

    @ExecSplit(default=(None,))
    def setRelativeMapPosition(self, position: Union[Vector2u, Pair[int], List[int]]) -> None:
        assert isinstance(position, (Vector2u, Tuple, List)), "position must be a tuple, list, or Vector2u"
        if isinstance(position, (tuple, list)):
            x, y = position
            position = Vector2u(x, y)
        self.setRelativePosition(Vector2f(position.x * GetCellSize(), position.y * GetCellSize()))

    @ExecSplit(default=(None,))
    def move(self, offset: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(offset, (Vector2f, Tuple, List)), "offset must be a tuple, list, or Vector2f"
        if isinstance(offset, (tuple, list)):
            x, y = offset
            offset = Vector2f(x, y)
        super().move(offset)
        self._relativePosition += offset
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()

    @ReturnType(angle=Angle)
    def getRotation(self) -> Angle:
        return super().getRotation()

    @ReturnType(angle=float)
    def v_getRotation(self) -> float:
        result = super().getRotation()
        return result.asDegrees()

    @ReturnType(angle=Angle)
    def getRelativeRotation(self) -> Angle:
        return self._relativeRotation

    @ReturnType(angle=float)
    def v_getRelativeRotation(self) -> float:
        return self._relativeRotation.asDegrees()

    @ExecSplit(default=(None,))
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

    @ExecSplit(default=(None,))
    def rotate(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        self._relativeRotation += angle
        super().rotate(angle)
        if self.getChildren():
            for child in self.getChildren():
                child._updateRotationFromParent()

    @ExecSplit(default=(None,))
    def setRelativeRotation(self, angle: Union[Angle, float]) -> None:
        if not isinstance(angle, Angle):
            angle = degrees(angle)
        parentRotation = degrees(0)
        if self.getParent():
            parentRotation = self.getParent().getRotation()
        self.setRotation(parentRotation + angle)

    @ReturnType(scale=Pair[float])
    def v_getScale(self) -> Pair[float]:
        result = super().getScale()
        return (result.x, result.y)

    @ReturnType(scale=Vector2f)
    def getScale(self) -> Vector2f:
        return super().getScale()

    @ReturnType(scale=Vector2f)
    def getRelativeScale(self) -> Vector2f:
        return self._relativeScale

    @ReturnType(scale=Pair[float])
    def v_getRelativeScale(self) -> Pair[float]:
        return (self._relativeScale.x, self._relativeScale.y)

    @ExecSplit(default=(None,))
    def setScale(self, scale: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(scale, (Vector2f, Tuple, List)), "scale must be a tuple, list, or Vector2f"
        if isinstance(scale, (tuple, list)):
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

    @ExecSplit(default=(None,))
    def scale(self, factor: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(factor, (Vector2f, Tuple, List)), "factor must be a tuple, list, or Vector2f"
        if isinstance(factor, (tuple, list)):
            x, y = factor
            factor = Vector2f(x, y)

        self._relativeScale = self._relativeScale.componentWiseMul(factor)
        super().scale(factor)
        if self.getChildren():
            for child in self.getChildren():
                child._updateScaleFromParent()

    @ExecSplit(default=(None,))
    def setRelativeScale(self, scale: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(scale, (Vector2f, Tuple, List)), "scale must be a tuple, list, or Vector2f"
        if isinstance(scale, (tuple, list)):
            x, y = scale
            scale = Vector2f(x, y)
        parentScale = Vector2f(1, 1)
        if self.getParent():
            parentScale = self.getParent().getScale()
        self.setScale(parentScale.componentWiseMul(scale))

    @ReturnType(origin=Pair[float])
    def v_getOrigin(self) -> Pair[float]:
        result = super().getOrigin()
        return (result.x, result.y)

    @ReturnType(origin=Vector2f)
    def getOrigin(self) -> Vector2f:
        return super().getOrigin()

    @ExecSplit(default=(None,))
    def setOrigin(self, origin: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(origin, (Vector2f, Tuple, List)), "origin must be a tuple, list, or Vector2f"
        if isinstance(origin, (tuple, list)):
            x, y = origin
            origin = Vector2f(x, y)
        return super().setOrigin(origin)

    @ReturnType(translation=Vector2f)
    def getTranslation(self) -> Vector2f:
        return self._translation

    @ExecSplit(default=(None,))
    def setTranslation(self, translation: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(translation, (Vector2f, Tuple, List)), "translation must be a tuple, list, or Vector2f"
        if isinstance(translation, (tuple, list)):
            translation = Vector2f(*translation)
        self._translation = translation
        self.setPosition(self.getPosition())

    @ExecSplit(default=(None,))
    def setAlignment(self, alignment: Tuple) -> None:
        x, y = alignment
        x = Utils.Math.Clamp(x, 0, 1)
        y = Utils.Math.Clamp(y, 0, 1)
        size = self.getTextureRect().size
        origin = Vector2f(size.x * x, size.y * y)
        self.setOrigin(origin)

    @ReturnType(map_="GameMap")
    def getMap(self) -> GameMap:
        return self._map

    @ExecSplit(default=(None,))
    def setMap(self, inMap: GameMap) -> None:
        self._map = inMap

    @ReturnType(parent=Optional["_ActorBase"])
    def getParent(self) -> Optional[_ActorBase]:
        return self._parent

    @ExecSplit(default=(None,))
    def setParent(self, parent: Optional[_ActorBase]) -> None:
        self._parent = parent

    @ReturnType(children=List["_ActorBase"])
    def getChildren(self) -> List[_ActorBase]:
        return self._children

    @ExecSplit(default=(None,))
    def addChild(self, child: _ActorBase) -> None:
        if child in self._children:
            warnings.warn("Child already exists")
            return
        self._children.append(child)
        child.setParent(self)
        if self._map:
            child.setMap(self._map)
            self._map.updateActorList()

    @ExecSplit(default=(None,))
    def removeChild(self, child: _ActorBase) -> None:
        if child not in self._children:
            raise ValueError("Child not found")
        self._children.remove(child)

    @ReturnType(visible=bool)
    def getVisible(self) -> bool:
        return self._visible

    @ExecSplit(default=(None,))
    def setVisible(self, visible: bool, applyToChildren: bool = True) -> None:
        self._visible = visible
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setVisible(visible, applyToChildren)

    @ReturnType(animatable=bool)
    def getAnimatable(self) -> bool:
        return self.animatable

    @ExecSplit(default=(None,))
    def setAnimatable(self, animate: bool, applyToChildren: bool = True) -> None:
        self.animatable = animate
        if applyToChildren:
            if self.getChildren():
                for child in self.getChildren():
                    child.setAnimatable(animate, applyToChildren)

    @ReturnType(texture=Optional["Texture"])
    def getSpriteTexture(self) -> Optional[Texture]:
        return super().getTexture()

    @ReturnType(texture=Optional["Texture"])
    def getTexture(self) -> Optional[Texture]:
        return self._texture

    @ExecSplit(default=(None,))
    def setSpriteTexture(self, texture: Texture, resetRect: bool = False) -> None:
        super().setTexture(texture, resetRect)

    @ExecSplit(default=(None,))
    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        self._texture = texture
        self.setSpriteTexture(texture, resetRect)

    @ReturnType(material=Material)
    def getMaterial(self) -> Material:
        return self.material

    @ExecSplit(default=(None,))
    def setMaterial(self, material: Material) -> None:
        self.material = material

    @ReturnType(lightBlock=float)
    def getLightBlock(self) -> float:
        return self.material.lightBlock

    @ExecSplit(default=(None,))
    def setLightBlock(self, lightBlock: float) -> None:
        self.material.lightBlock = lightBlock

    @ReturnType(mirror=bool)
    def getMirror(self) -> bool:
        return self.material.mirror

    @ExecSplit(default=(None,))
    def setMirror(self, mirror: bool) -> None:
        self.material.mirror = mirror

    @ReturnType(reflectionStrength=float)
    def getReflectionStrength(self) -> float:
        return self.material.reflectionStrength

    @ExecSplit(default=(None,))
    def setReflectionStrength(self, reflectionStrength: float) -> None:
        self.material.reflectionStrength = reflectionStrength

    @ReturnType(opacity=float)
    def getOpacity(self) -> float:
        return self.material.opacity

    @ExecSplit(default=(None,))
    def setOpacity(self, opacity: float) -> None:
        self.material.opacity = opacity

    @ReturnType(emissive=float)
    def getEmissive(self) -> float:
        return self.material.emissive

    @ExecSplit(default=(None,))
    def setEmissive(self, emissive: float) -> None:
        self.material.emissive = emissive

    @ReturnType(emissive=float)
    def getEmissive(self) -> float:
        return self.material.emissive

    @ExecSplit(default=(None,))
    def setEmissive(self, emissive: float) -> None:
        self.material.emissive = emissive

    @ExecSplit(default=(None,))
    def setGraph(self, graph: Graph) -> None:
        self._graph = graph

    def _superMove(self, offset: Union[Vector2f, Pair[float], List[float]]) -> None:
        assert isinstance(offset, (Vector2f, Tuple, List)), "offset must be a tuple, list, or Vector2f"
        if isinstance(offset, (tuple, list)):
            offset = Vector2f(*offset)
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

    def _animate(self, deltaTime: float) -> None:
        newRect = copy.copy(self.getTextureRect())
        rectWidth = newRect.size.x
        rectX = newRect.position.x
        textureWidth = self._texture.getSize().x
        newRectX = (rectX + rectWidth) % textureWidth
        newRect.position.x = newRectX
        self._switchTimer += deltaTime
        if self._switchTimer >= self.switchInterval:
            self._switchTimer = 0.0
            self.setTextureRect(newRect)
