# -*- encoding: utf-8 -*-

from .. import RenderStates, Vector2f
from .Base import SpriteBase


class Image(SpriteBase):
    r"""Image widget backed by a textured sprite.

    Inherits from SpriteBase and applies scale transform during rendering.
    """

    def _applyRenderStates(self, states: RenderStates) -> None:
        r"""\brief Apply scale transform to render states.

        - \param states  Render states to modify
        """
        from .. import Scale

        super()._applyRenderStates(states)
        states.transform.scale(Vector2f(Scale, Scale))
