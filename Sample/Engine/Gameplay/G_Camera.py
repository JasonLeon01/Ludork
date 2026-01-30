# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, TYPE_CHECKING
from .. import (
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
)
from ..Utils import Math, Render

if TYPE_CHECKING:
    from Engine import Texture, Image
    from Engine.Gameplay import GameMap
    from Engine.Gameplay.Actors import Actor


class Camera(Drawable):
    def __init__(self, viewport: Optional[FloatRect] = None) -> None:
        super().__init__()
        self._viewport = viewport
        if self._viewport is None:
            from .. import System

            self._viewport = FloatRect(Vector2f(0, 0), Math.ToVector2f(System.getGameSize()))
        self._renderTexture: RenderTexture
        self._renderBlockableTexture: RenderTexture
        assert isinstance(self._viewport, FloatRect)
        size = Math.ToVector2u(self._viewport.size)
        self._renderTexture = RenderTexture(size)
        self._renderBlockableTexture = RenderTexture(size)
        self._renderTexture.setView(View(self._viewport))
        self._renderBlockableTexture.setView(View(self._viewport))
        self._renderSprite = Sprite(self._renderTexture.getTexture())
        self._renderBlockableSprite = Sprite(self._renderBlockableTexture.getTexture())
        self._renderStates = Render.CanvasRenderStates()
        self._parent: Optional[Actor] = None
        self._map: Optional[GameMap] = None

    def getViewport(self) -> FloatRect:
        return self._viewport

    def setViewport(self, inViewport: FloatRect) -> None:
        assert isinstance(inViewport, FloatRect)
        self._viewport = inViewport
        self._refreshView()

    def getView(self) -> View:
        return self._renderTexture.getView()

    def getViewPosition(self) -> Vector2f:
        return self._viewport.position

    def setViewPosition(self, inPosition: Vector2f) -> None:
        self._viewport.position = inPosition
        self._refreshView()

    def getViewSize(self) -> Vector2f:
        return self._viewport.size

    def setViewSize(self, inSize: Vector2f) -> None:
        self._viewport.size = inSize
        self._refreshView()

    def getViewRotation(self) -> Angle:
        return self._renderTexture.getView().getRotation()

    def v_getViewRotation(self) -> float:
        return self._renderTexture.getView().getRotation().asDegrees()

    def setViewRotation(self, inRotation: Union[Angle, float]) -> None:
        assert isinstance(inRotation, (Angle, float))
        view = self._renderTexture.getView()
        if isinstance(inRotation, float):
            inRotation = degrees(inRotation)
        view.setRotation(inRotation)
        self._renderTexture.setView(view)
        self._renderBlockableTexture.setView(view)

    def moveView(self, delta: Vector2f) -> None:
        self.setViewPosition(self._viewport.position + delta)
        self.fixViewPosition()

    def rotateView(self, delta: float) -> None:
        self.setViewRotation(self.getViewRotation() + degrees(delta))

    def getRenderStates(self) -> RenderStates:
        return self._renderStates

    def setRenderStates(self, inRenderStates: RenderStates) -> None:
        self._renderStates = inRenderStates

    def resumeViewport(self) -> None:
        self._renderTexture.setView(self._renderTexture.getDefaultView())
        self._renderBlockableTexture.setView(self._renderBlockableTexture.getDefaultView())

    def getPosition(self) -> Vector2f:
        return self._renderSprite.getPosition()

    def setPosition(self, inPosition: Vector2f) -> None:
        self._renderSprite.setPosition(inPosition)
        self._renderBlockableSprite.setPosition(inPosition)

    def getRotation(self) -> Angle:
        return self._renderSprite.getRotation()

    def v_getRotation(self) -> float:
        return self._renderSprite.getRotation().asDegrees()

    def setRotation(self, inRotation: Union[Angle, float]) -> None:
        assert isinstance(inRotation, (Angle, float))
        if isinstance(inRotation, float):
            inRotation = degrees(inRotation)
        self._renderSprite.setRotation(inRotation)
        self._renderBlockableSprite.setRotation(inRotation)

    def getScale(self) -> Vector2f:
        return self._renderSprite.getScale()

    def setScale(self, inScale: Vector2f) -> None:
        self._renderSprite.setScale(inScale)
        self._renderBlockableSprite.setScale(inScale)

    def mapPixelToCoords(self, point):
        return self._renderTexture.mapPixelToCoords(point, self._renderTexture.getView())

    def mapCoordsToPixel(self, point):
        return self._renderTexture.mapCoordsToPixel(point, self._renderTexture.getView())

    def getTexture(self) -> Texture:
        return self._renderTexture.getTexture()

    def getBlockableTexture(self) -> Texture:
        return self._renderBlockableTexture.getTexture()

    def getImage(self) -> Image:
        return self._renderTexture.getTexture().copyToImage()

    def getBlockableImage(self) -> Image:
        return self._renderBlockableTexture.getTexture().copyToImage()

    def setParent(self, actor: Actor) -> None:
        self._parent = actor

    def getParent(self) -> Optional[Actor]:
        return self._parent

    def setMap(self, map: GameMap) -> None:
        self._map = map

    def getMap(self) -> Optional[GameMap]:
        return self._map

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
        target.draw(self._renderSprite, states)

    def clear(self) -> None:
        self._renderTexture.clear(Color.Transparent)
        self._renderBlockableTexture.clear(Color.Transparent)

    def render(self, object: Drawable) -> None:
        self._renderTexture.draw(object, self._renderStates)

    def renderBlockable(self, object: Drawable) -> None:
        self._renderBlockableTexture.draw(object, self._renderStates)

    def display(self):
        self._renderTexture.display()

    def displayBlockable(self):
        self._renderBlockableTexture.display()

    def _refreshView(self) -> None:
        self._renderTexture.setView(View(self._viewport))
        self._renderBlockableTexture.setView(View(self._viewport))
