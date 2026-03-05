# -*- encoding: utf-8 -*-

from .. import RenderStates, Vector2f, System
from .Base import SpriteBase


class Image(SpriteBase):
    def _applyRenderStates(self, states: RenderStates) -> None:
        super()._applyRenderStates(states)
        states.transform.scale(Vector2f(System.getScale(), System.getScale()))
