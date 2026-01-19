# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from ... import RenderTarget, Sprite, Vector2f, RenderStates, Texture, IntRect, FloatRect
from .UIB_ControlBase import ControlBase


class SpriteBase(ControlBase):
    def __init__(self, texture: Texture, rect: Optional[IntRect] = None) -> None:
        from ...Utils import Render

        self._sprite: Sprite
        self._renderStates: RenderStates = Render.CanvasRenderStates()
        if rect:
            self._sprite = Sprite(texture, rect)
        else:
            self._sprite = Sprite(texture)
        super().__init__()

    def getLocalBounds(self) -> FloatRect:
        return self._sprite.getLocalBounds()

    def getGlobalBounds(self) -> FloatRect:
        from ... import System

        transform = self.getTransform()
        transform.translate(Vector2f(System.getScale() - 1, System.getScale() - 1))
        return transform.transformRect(self.getLocalBounds())

    def draw(self, target: RenderTarget, states: RenderStates) -> None:
        self._applyRenderStates(states)
        target.draw(self._sprite, states)

    def _applyRenderStates(self, states: RenderStates) -> None:
        from ... import System

        states.transform *= self.getTransform()
        states.transform.translate(Vector2f(System.getScale() - 1, System.getScale() - 1))
