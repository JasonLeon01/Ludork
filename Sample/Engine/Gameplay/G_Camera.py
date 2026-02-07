# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, TYPE_CHECKING
from .. import (
    Pair,
    Transformable,
    Drawable,
    RenderTexture,
    Sprite,
    FloatRect,
    Vector2f,
    View,
    RenderTarget,
    RenderStates,
    Color,
    Angle,
    degrees,
    GetCellSize,
    ReturnType,
    ExecSplit,
)
from ..Utils import Math, Render

if TYPE_CHECKING:
    from Engine import Texture, Image
    from Engine.Gameplay import GameMap
    from Engine.Gameplay.Actors import Actor


class Camera(Drawable, Transformable):
    def __init__(self, viewport: Optional[FloatRect] = None) -> None:
        Drawable.__init__(self)
        Transformable.__init__(self)
        self._viewport = viewport
        if self._viewport is None:
            from .. import System

            self._viewport = FloatRect(Vector2f(0, 0), Math.ToVector2f(System.getGameSize()))
        self._renderTexture: RenderTexture
        assert isinstance(self._viewport, FloatRect)
        size = Math.ToVector2u(self._viewport.size)
        self._renderTexture = RenderTexture(size)
        self._renderTexture.setView(View(self._viewport))
        self._renderSprite = Sprite(self._renderTexture.getTexture())
        self._canvases: List[RenderTexture] = []
        self._renderStates = Render.CanvasRenderStates()
        self._parent: Optional[Actor] = None
        self._map: Optional[GameMap] = None

    @ExecSplit(default=(None,))
    def setViewport(self, inViewport: FloatRect) -> None:
        assert isinstance(inViewport, FloatRect)
        self._viewport = inViewport
        self._refreshView()

    @ReturnType(view=View)
    def getView(self) -> View:
        return self._renderTexture.getView()

    @ReturnType(position=Vector2f)
    def getViewPosition(self) -> Vector2f:
        return self._viewport.position

    @ReturnType(position=Pair[float])
    def v_getViewPosition(self) -> Pair[float]:
        return (self._viewport.position.x, self._viewport.position.y)

    @ExecSplit(default=(None,))
    def setViewPosition(self, inPosition: Vector2f) -> None:
        self._viewport.position = inPosition
        self._refreshView()

    @ReturnType(size=Vector2f)
    def getViewSize(self) -> Vector2f:
        return self._viewport.size

    @ReturnType(size=Pair[float])
    def v_getViewSize(self) -> Pair[float]:
        return (self._viewport.size.x, self._viewport.size.y)

    @ExecSplit(default=(None,))
    def setViewSize(self, inSize: Vector2f) -> None:
        self._viewport.size = inSize
        self._refreshView()

    @ReturnType(rotation=Angle)
    def getViewRotation(self) -> Angle:
        return self._renderTexture.getView().getRotation()

    @ReturnType(rotation=float)
    def v_getViewRotation(self) -> float:
        return self._renderTexture.getView().getRotation().asDegrees()

    @ExecSplit(default=(None,))
    def setViewRotation(self, inRotation: Union[Angle, float]) -> None:
        assert isinstance(inRotation, (Angle, float))
        view = self._renderTexture.getView()
        if isinstance(inRotation, float):
            inRotation = degrees(inRotation)
        view.setRotation(inRotation)
        self._renderTexture.setView(view)
        for canvas in self._canvases:
            canvas.setView(view)

    @ExecSplit(default=(None,))
    def moveView(self, delta: Union[Vector2f, Pair[float]]) -> None:
        assert isinstance(delta, (Vector2f, tuple))
        if isinstance(delta, tuple):
            delta = Vector2f(*delta)
        self.setViewPosition(self._viewport.position + delta)

    @ExecSplit(default=(None,))
    def rotateView(self, delta: Union[Angle, float]) -> None:
        assert isinstance(delta, (Angle, float))
        if isinstance(delta, float):
            delta = degrees(delta)
        self.setViewRotation(self.getViewRotation() + delta)

    @ExecSplit(default=(None,))
    def resumeViewport(self) -> None:
        self._renderTexture.setView(self._renderTexture.getDefaultView())

    @ReturnType(position=Pair[float])
    def v_getPosition(self) -> Pair[float]:
        pos = self.getPosition()
        return (pos.x, pos.y)

    @ReturnType(position=Vector2f)
    def getPosition(self) -> Vector2f:
        return super().getPosition()

    @ExecSplit(default=(None,))
    def setPosition(self, inPosition: Union[Vector2f, Pair[float]]) -> None:
        assert isinstance(inPosition, (Vector2f, tuple))
        if isinstance(inPosition, tuple):
            inPosition = Vector2f(*inPosition)
        super().setPosition(inPosition)

    @ExecSplit(default=(None,))
    def move(self, delta: Union[Vector2f, Pair[float]]) -> None:
        assert isinstance(delta, (Vector2f, tuple))
        if isinstance(delta, tuple):
            delta = Vector2f(*delta)
        super().move(delta)

    @ReturnType(rotation=float)
    def v_getRotation(self) -> float:
        return self.getRotation().asDegrees()

    @ReturnType(rotation=Angle)
    def getRotation(self) -> Angle:
        return super().getRotation()

    @ExecSplit(default=(None,))
    def setRotation(self, inRotation: Union[Angle, float]) -> None:
        assert isinstance(inRotation, (Angle, float))
        if isinstance(inRotation, float):
            inRotation = degrees(inRotation)
        super().setRotation(inRotation)

    @ExecSplit(default=(None,))
    def rotate(self, delta: Union[Angle, float]) -> None:
        assert isinstance(delta, (Angle, float))
        if isinstance(delta, float):
            delta = degrees(delta)
        super().rotate(delta)

    @ReturnType(scale=Pair[float])
    def v_getScale(self) -> Pair[float]:
        scale = self.getScale()
        return (scale.x, scale.y)

    @ReturnType(scale=Vector2f)
    def getScale(self) -> Vector2f:
        return super().getScale()

    @ExecSplit(default=(None,))
    def setScale(self, inScale: Union[Vector2f, Pair[float]]) -> None:
        assert isinstance(inScale, (Vector2f, tuple))
        if isinstance(inScale, tuple):
            inScale = Vector2f(*inScale)
        super().setScale(inScale)

    @ExecSplit(default=(None,))
    def scale(self, delta: Union[Vector2f, Pair[float]]) -> None:
        assert isinstance(delta, (Vector2f, tuple))
        if isinstance(delta, tuple):
            delta = Vector2f(*delta)
        super().scale(delta)

    @ReturnType(parent="Actor")
    def getParent(self) -> Optional[Actor]:
        return self._parent

    @ExecSplit(default=(None,))
    def setParent(self, actor: Actor) -> None:
        self._parent = actor

    @ExecSplit(default=(None,))
    def setMap(self, map: GameMap) -> None:
        self._map = map

    @ReturnType(map="GameMap")
    def getMap(self) -> Optional[GameMap]:
        return self._map

    def applyCanvasCount(self, count: int) -> None:
        if count < len(self._canvases):
            self._canvases = self._canvases[:count]
        else:
            for i in range(len(self._canvases), count):
                rt = RenderTexture(Math.ToVector2u(self._viewport.size))
                rt.setView(self._renderTexture.getView())
                self._canvases.append(rt)

    def getCanvases(self) -> List[RenderTexture]:
        return self._canvases

    def getViewport(self) -> FloatRect:
        return self._viewport

    def getRenderStates(self) -> RenderStates:
        return self._renderStates

    def setRenderStates(self, inRenderStates: RenderStates) -> None:
        self._renderStates = inRenderStates

    def setScale(self, inScale: Union[Vector2f, Pair[float]]) -> None:
        assert isinstance(inScale, (Vector2f, tuple))
        if isinstance(inScale, tuple):
            inScale = Vector2f(*inScale)
        super().setScale(inScale)

    def mapPixelToCoords(self, point):
        return self._renderTexture.mapPixelToCoords(point, self._renderTexture.getView())

    def mapCoordsToPixel(self, point):
        return self._renderTexture.mapCoordsToPixel(point, self._renderTexture.getView())

    def getTexture(self) -> Texture:
        return self._renderTexture.getTexture()

    def getImage(self) -> Image:
        return self._renderTexture.getTexture().copyToImage()

    def onTick(self, deltaTime: float) -> None:
        pass

    def onLateTick(self, deltaTime: float) -> None:
        pass

    def onFixedTick(self, fixedDelta: float) -> None:
        if self._map is None:
            return
        if self._parent:
            self.setViewPosition(self._parent.getPosition() - self._viewport.size / 2)
            self.fixViewPosition()

    def fixViewPosition(self) -> None:
        if self._map is None:
            return
        pos = self.getViewPosition()
        mapSize = self._map.getSize()
        maxX = mapSize.x * GetCellSize() - self._viewport.size.x
        maxY = mapSize.y * GetCellSize() - self._viewport.size.y
        px = Math.Clamp(pos.x, 0, maxX if maxX > 0 else 0)
        py = Math.Clamp(pos.y, 0, maxY if maxY > 0 else 0)
        self.setViewPosition(Vector2f(px, py))

    def draw(self, target: RenderTarget, states: RenderStates = RenderStates()) -> None:
        states.transform *= self.getTransform()
        target.draw(self._renderSprite, states)

    def clear(self) -> None:
        self._renderTexture.clear(Color.Transparent)

    def display(self):
        self._renderTexture.display()

    def _refreshView(self) -> None:
        view = View(self._viewport)
        self._renderTexture.setView(view)
        for canvas in self._canvases:
            canvas.setView(view)
