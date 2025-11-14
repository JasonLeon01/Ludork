# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, TYPE_CHECKING, Union
from . import Drawable, RenderTexture, Sprite, FloatRect, View, RenderTarget, RenderStates, Color, Angle, degrees
from ..Utils import U_Math, U_Render

if TYPE_CHECKING:
    from Engine import Vector2f

ToVector2f = U_Math.ToVector2f
ToVector2u = U_Math.ToVector2u
CanvasRenderStates = U_Render.CanvasRenderStates


class Camera(Drawable):
    def __init__(self, viewport: Optional[FloatRect] = None) -> None:
        super().__init__()
        self._viewport = viewport
        if self._viewport is None:
            from Engine import System

            self._viewport = FloatRect(ToVector2f(System.getGameSize() / 2), ToVector2f(System.getGameSize()))
        self._renderTexture: RenderTexture
        assert isinstance(self._viewport, FloatRect)
        self._renderTexture = RenderTexture(ToVector2u(self._viewport.size))
        self._renderTexture.setView(View(self._viewport))
        self._renderSprite = Sprite(self._renderTexture.getTexture())
        self._renderStates = CanvasRenderStates()

    def getViewport(self) -> FloatRect:
        return self._viewport

    def setViewport(self, inViewport: FloatRect) -> None:
        assert isinstance(inViewport, FloatRect)
        self._viewport = inViewport
        self._refreshView()

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

    def moveView(self, delta: Vector2f) -> None:
        self.setViewPosition(self._viewport.position + delta)

    def rotateView(self, delta: float) -> None:
        self.setViewRotation(self.getViewRotation() + degrees(delta))

    def getRenderStates(self) -> RenderStates:
        return self._renderStates

    def setRenderStates(self, inRenderStates: RenderStates) -> None:
        self._renderStates = inRenderStates

    def resumeViewport(self) -> None:
        self._renderTexture.setView(self._renderTexture.getDefaultView())

    def getPosition(self) -> Vector2f:
        return self._renderSprite.getPosition()

    def setPosition(self, inPosition: Vector2f) -> None:
        self._renderSprite.setPosition(inPosition)

    def getRotation(self) -> Angle:
        return self._renderSprite.getRotation()

    def v_getRotation(self) -> float:
        return self._renderSprite.getRotation().asDegrees()

    def setRotation(self, inRotation: Union[Angle, float]) -> None:
        assert isinstance(inRotation, (Angle, float))
        if isinstance(inRotation, float):
            inRotation = degrees(inRotation)
        self._renderSprite.setRotation(inRotation)

    def getScale(self) -> Vector2f:
        return self._renderSprite.getScale()

    def setScale(self, inScale: Vector2f) -> None:
        self._renderSprite.setScale(inScale)

    def mapPixelToCoords(self, point):
        return self._renderTexture.mapPixelToCoords(point, self._renderTexture.getView())

    def mapCoordsToPixel(self, point):
        return self._renderTexture.mapCoordsToPixel(point, self._renderTexture.getView())

    def draw(self, target: RenderTarget, states: RenderStates = RenderStates()) -> None:
        self._renderTexture.display()
        target.draw(self._renderSprite, states)

    def clear(self) -> None:
        self._renderTexture.clear(Color.Transparent)

    def render(self, object: Drawable) -> None:
        self._renderTexture.draw(object, self._renderStates)

    def _refreshView(self) -> None:
        self._renderTexture.setView(view)
