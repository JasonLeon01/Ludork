# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, List, TYPE_CHECKING
from .. import (
    TypeAdapter,
    Pair,
    Vector2u,
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
    @TypeAdapter(inRotation=(float, Angle, degrees))
    def setViewRotation(self, inRotation: Union[Angle, float]) -> None:
        view = self._renderTexture.getView()
        view.setRotation(inRotation)
        self._renderTexture.setView(view)
        for canvas in self._canvases:
            canvas.setView(view)

    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(tuple, Vector2f))
    def moveView(self, delta: Union[Vector2f, Pair[float]]) -> None:
        self.setViewPosition(self._viewport.position + delta)

    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(float, Angle, degrees))
    def rotateView(self, delta: Union[Angle, float]) -> None:
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
    @TypeAdapter(inPosition=(tuple, Vector2f))
    def setPosition(self, inPosition: Union[Vector2f, Pair[float]]) -> None:
        super().setPosition(inPosition)

    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(tuple, Vector2f))
    def move(self, delta: Union[Vector2f, Pair[float]]) -> None:
        super().move(delta)

    @ReturnType(rotation=float)
    def v_getRotation(self) -> float:
        return self.getRotation().asDegrees()

    @ReturnType(rotation=Angle)
    def getRotation(self) -> Angle:
        return super().getRotation()

    @ExecSplit(default=(None,))
    @TypeAdapter(inRotation=(float, Angle, degrees))
    def setRotation(self, inRotation: Union[Angle, float]) -> None:
        super().setRotation(inRotation)

    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(float, Angle, degrees))
    def rotate(self, delta: Union[Angle, float]) -> None:
        super().rotate(delta)

    @ReturnType(scale=Pair[float])
    def v_getScale(self) -> Pair[float]:
        scale = self.getScale()
        return (scale.x, scale.y)

    @ReturnType(scale=Vector2f)
    def getScale(self) -> Vector2f:
        return super().getScale()

    @ExecSplit(default=(None,))
    @TypeAdapter(inScale=(tuple, Vector2f))
    def setScale(self, inScale: Union[Vector2f, Pair[float]]) -> None:
        super().setScale(inScale)

    @ExecSplit(default=(None,))
    @TypeAdapter(delta=(tuple, Vector2f))
    def scale(self, delta: Union[Vector2f, Pair[float]]) -> None:
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

    def initLightMask(self, size: Vector2u) -> RenderTexture:
        self._lightMask = RenderTexture(size)
        return self._lightMask

    def render(self, obj: Drawable) -> None:
        self._renderTexture.draw(obj, self._renderStates)

    def getViewport(self) -> FloatRect:
        return self._viewport

    def getRenderStates(self) -> RenderStates:
        return self._renderStates

    def setRenderStates(self, inRenderStates: RenderStates) -> None:
        self._renderStates = inRenderStates

    @TypeAdapter(inScale=(tuple, Vector2f))
    def setScale(self, inScale: Union[Vector2f, Pair[float]]) -> None:
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
