# -*- encoding: utf-8 -*-

from . import UI_SpriteBase, RenderStates, Vector2f

SpriteBase = UI_SpriteBase.SpriteBase


class Image(SpriteBase):
    def _applyRenderState(self, states: RenderStates) -> None:
        from Engine import System

        super()._applyRenderState(states)
        states.transform.scale(Vector2f(System.getScale(), System.getScale()))
