# -*- encoding: utf-8 -*-

from .. import RenderStates, Vector2f
from .Base import SpriteBase


class Image(SpriteBase):
    def _applyRenderStates(self, states: RenderStates) -> None:
        from .. import Scale

        super()._applyRenderStates(states)
        states.transform.scale(Vector2f(Scale, Scale))
