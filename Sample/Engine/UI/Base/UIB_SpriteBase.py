# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from ... import (
    Sprite,
    Vector2f,
    RenderStates,
    Texture,
    IntRect,
    FloatRect,
    Transform,
    Texture,
    IntRect,
    RenderStates,
    Color,
    FloatRect,
    RenderTarget,
)
from .UIB_ControlBase import ControlBase


class SpriteBase(ControlBase):
    def __init__(self, texture: Texture, rect: Optional[IntRect] = None) -> None:
        from ...Utils import Render

        self._sprite: Sprite
        self._texture = texture
        self._renderStates: RenderStates = Render.CanvasRenderStates()
        if rect:
            self._sprite = Sprite(texture, rect)
        else:
            self._sprite = Sprite(texture)
        super().__init__()

    def setTexture(self, texture: Texture, resetRect: bool = False) -> None:
        self._texture = texture
        self._sprite.setTexture(texture, resetRect)

    def getTexture(self) -> Texture:
        return self._sprite.getTexture()

    def setTextureRect(self, rect: IntRect) -> None:
        self._sprite.setTextureRect(rect)

    def getTextureRect(self) -> IntRect:
        return self._sprite.getTextureRect()

    def setColor(self, color: Color) -> None:
        self._sprite.setColor(color)

    def getColor(self) -> Color:
        return self._sprite.getColor()

    def getSize(self) -> Vector2f:
        return self._sprite.getGlobalBounds().size

    def getLocalBounds(self) -> FloatRect:
        from ... import System

        bounds = self._sprite.getLocalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / System.getScale())
        return newBounds

    def getGlobalBounds(self) -> FloatRect:
        from ... import System

        bounds = self._sprite.getGlobalBounds()
        newBounds = FloatRect(bounds.position, bounds.size / System.getScale())
        return newBounds

    def getRenderStates(self) -> RenderStates:
        return self._renderStates

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        self._applyRenderStates(states)
        target.draw(self._sprite, states)

    def _applyRenderStates(self, states: RenderStates) -> None:
        from ... import System

        states.transform.translate(self.getPosition() * (System.getScale() - 1))
        states.transform *= self.getTransform()

    def _getRenderTransform(self) -> Transform:
        from ... import System

        transform = Transform()
        transform.translate(self.getPosition() * (System.getScale() - 1))
        transform *= self.getTransform()
        return transform

    def getAbsoluteBounds(self) -> FloatRect:
        from ... import System

        transform = self._getScreenRenderTransform()
        bounds = self.getLocalBounds()
        realBounds = FloatRect(bounds.position * System.getScale(), bounds.size * System.getScale())
        return transform.transformRect(realBounds)
