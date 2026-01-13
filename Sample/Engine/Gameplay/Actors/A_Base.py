# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
import warnings
from typing import List, Optional, Tuple, Union, TYPE_CHECKING
from . import Sprite, IntRect, Vector2i, Vector2u, Vector2f, Angle, degrees, GetCellSize, Utils, ExecSplit, ReturnType

if TYPE_CHECKING:
    from Engine import Texture
    from Engine.Gameplay import GameMap
    from Engine.NodeGraph import Graph


class ActorBase(Sprite):
    def __init__(
        self,
        texture: Optional[Texture] = None,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]] = None,
        tag: Optional[str] = None,
    ) -> None:
        if not rect is None:
            assert isinstance(rect, (IntRect, tuple)), "rect must be a tuple or IntRect"
            if not isinstance(rect, IntRect):
                assert len(rect) == 2, "rect must be a tuple of two tuples"
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

        self.switchInterval: float = 0.2
        self.tag: str = ""
        if not tag is None:
            self.tag = tag
        self.animatable: bool = False

        self._map: GameMap = None
        self._parent: Optional[ActorBase] = None
        self._children: List[ActorBase] = []
        self._visible: bool = True
        self._texture: Optional[Texture] = texture
        self._switchTimer: float = 0.0
        self._relativePosition: Vector2f = Vector2f(0, 0)
        self._relativeRotation: Angle = degrees(0)
        self._relativeScale: Vector2f = Vector2f(1, 1)
        self._lightBlock: float = 0.5
        self._graph: Optional[Graph] = None

    def update(self, deltaTime: float) -> None:
        if self.animatable:
            self._animate(deltaTime)

    def lateUpdate(self, deltaTime: float) -> None:
        pass

    def fixedUpdate(self, fixedDelta: float) -> None:
        pass

    @ReturnType(pos=Tuple[float, float])
    def v_getPosition(self) -> Tuple[float, float]:
        result = super().getPosition()
        return (result.x, result.y)

    @ReturnType(pos=Vector2f)
    def getRelativePosition(self) -> Vector2f:
        return self._relativePosition

    @ReturnType(pos=Tuple[float, float])
    def v_getRelativePosition(self) -> Tuple[float, float]:
        return (self._relativePosition.x, self._relativePosition.y)

    @ReturnType(pos=Vector2i)
    def getMapPosition(self) -> Vector2i:
        return Vector2i(
            int(self.getPosition().x * 1.0 / GetCellSize() + 0.5),
            int(self.getPosition().y * 1.0 / GetCellSize() + 0.5),
        )

    @ReturnType(pos=Tuple[int, int])
    def v_getMapPosition(self) -> Tuple[int, int]:
        return (self.getMapPosition().x, self.getMapPosition().y)

    @ReturnType(pos=Vector2i)
    def getRelativeMapPosition(self) -> Vector2i:
        return Vector2i(
            int(self.getRelativePosition().x / GetCellSize()),
            int(self.getRelativePosition().y / GetCellSize()),
        )

    @ReturnType(pos=Tuple[int, int])
    def v_getRelativeMapPosition(self) -> Tuple[int, int]:
        return (self.getRelativeMapPosition().x, self.getRelativeMapPosition().y)

    @ExecSplit(default=(None,))
    def setPosition(self, position: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(position, (Vector2f, tuple)), "position must be a tuple or Vector2f"
        if not isinstance(position, Vector2f):
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

    @ExecSplit(default=(None,))
    def setRelativePosition(self, position: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(position, (Vector2f, tuple)), "position must be a tuple or Vector2f"
        if not isinstance(position, Vector2f):
            x, y = position
            position = Vector2f(x, y)
        parentPosition = Vector2f(0, 0)
        if self.getParent():
            parentPosition = self.getParent().getPosition()
        self.setPosition(parentPosition + position)

    @ExecSplit(default=(None,))
    def setMapPosition(self, position: Union[Vector2u, Tuple[int, int]]) -> None:
        assert isinstance(position, (Vector2u, tuple)), "position must be a tuple or Vector2u"
        if not isinstance(position, Vector2u):
            x, y = position
            position = Vector2u(x, y)
        self.setPosition(Vector2f(position.x * GetCellSize(), position.y * GetCellSize()))

    @ExecSplit(default=(None,))
    def setRelativeMapPosition(self, position: Union[Vector2u, Tuple[int, int]]) -> None:
        assert isinstance(position, (Vector2u, tuple)), "position must be a tuple or Vector2u"
        if not isinstance(position, Vector2u):
            x, y = position
            position = Vector2u(x, y)
        self.setRelativePosition(Vector2f(position.x * GetCellSize(), position.y * GetCellSize()))

    @ExecSplit(default=(None,))
    def move(self, offset: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(offset, (Vector2f, tuple)), "offset must be a tuple or Vector2f"
        if not isinstance(offset, Vector2f):
            x, y = offset
            offset = Vector2f(x, y)
        super().move(offset)
        self._relativePosition += offset
        if self.getChildren():
            for child in self.getChildren():
                child._updatePositionFromParent()

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

    @ReturnType(scale=Tuple[float, float])
    def v_getScale(self) -> Tuple[float, float]:
        result = super().getScale()
        return (result.x, result.y)

    @ReturnType(scale=Vector2f)
    def getRelativeScale(self) -> Vector2f:
        return self._relativeScale

    @ReturnType(scale=Tuple[float, float])
    def v_getRelativeScale(self) -> Tuple[float, float]:
        return (self._relativeScale.x, self._relativeScale.y)

    @ExecSplit(default=(None,))
    def setScale(self, scale: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(scale, (Vector2f, tuple)), "scale must be a tuple or Vector2f"
        if not isinstance(scale, Vector2f):
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
    def scale(self, factor: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(factor, (Vector2f, tuple)), "factor must be a tuple or Vector2f"
        if not isinstance(factor, Vector2f):
            x, y = factor
            factor = Vector2f(x, y)

        self._relativeScale = self._relativeScale.componentWiseMul(factor)
        super().scale(factor)
        if self.getChildren():
            for child in self.getChildren():
                child._updateScaleFromParent()

    @ExecSplit(default=(None,))
    def setRelativeScale(self, scale: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(scale, (Vector2f, tuple)), "scale must be a tuple or Vector2f"
        if not isinstance(scale, Vector2f):
            x, y = scale
            scale = Vector2f(x, y)
        parentScale = Vector2f(1, 1)
        if self.getParent():
            parentScale = self.getParent().getScale()
        self.setScale(parentScale.componentWiseMul(scale))

    @ReturnType(origin=Tuple[float, float])
    def v_getOrigin(self) -> Tuple[float, float]:
        result = super().getOrigin()
        return (result.x, result.y)

    @ExecSplit(default=(None,))
    def setOrigin(self, origin: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(origin, (Vector2f, tuple)), "origin must be a tuple or Vector2f"
        if not isinstance(origin, Vector2f):
            x, y = origin
            origin = Vector2f(x, y)
        return super().setOrigin(origin)

    @ExecSplit(default=(None,))
    def setAlignment(self, alignment: Tuple[float, float]) -> None:
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

    @ReturnType(parent=Optional["ActorBase"])
    def getParent(self) -> Optional[ActorBase]:
        return self._parent

    @ExecSplit(default=(None,))
    def setParent(self, parent: Optional[ActorBase]) -> None:
        self._parent = parent

    @ReturnType(children=List["ActorBase"])
    def getChildren(self) -> List[ActorBase]:
        return self._children

    @ExecSplit(default=(None,))
    def addChild(self, child: ActorBase) -> None:
        if child in self._children:
            warnings.warn("Child already exists")
            return
        self._children.append(child)
        child.setParent(self)
        if self._map:
            child.setMap(self._map)
            self._map.updateActorList()

    @ExecSplit(default=(None,))
    def removeChild(self, child: ActorBase) -> None:
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

    @ReturnType(lightBlock=float)
    def getLightBlock(self) -> float:
        return self._lightBlock

    @ExecSplit(default=(None,))
    def setLightBlock(self, lightBlock: float) -> None:
        self._lightBlock = lightBlock

    @ExecSplit(default=(None,))
    def setGraph(self, graph: Graph) -> None:
        self._graph = graph

    def _superMove(self, offset: Union[Vector2f, Tuple[float, float]]) -> None:
        assert isinstance(offset, (Vector2f, tuple)), "offset must be a tuple or Vector2f"
        if not isinstance(offset, Vector2f):
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
