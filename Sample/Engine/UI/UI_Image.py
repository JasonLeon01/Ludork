# -*- encoding: utf-8 -*-

from .. import RenderStates, Vector2f
from .Base import SpriteBase


class Image(SpriteBase):
    def _applyRenderStates(self, states: RenderStates) -> None:
        from .. import System

        super()._applyRenderStates(states)
        states.transform.scale(Vector2f(System.getScale(), System.getScale()))
