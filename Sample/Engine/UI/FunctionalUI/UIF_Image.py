# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from ... import Texture, IntRect
from .. import Image
from ..Base import FunctionalBase


class FunctionalImage(Image, FunctionalBase):
    r"""Interactive image control with hover/click callback support.

    Inherits from Image and FunctionalBase.
    """

    def __init__(self, texture: Texture, rect: Optional[IntRect] = None) -> None:
        r"""\brief Construct a FunctionalImage.

        - \param texture  Texture used for the image
        - \param rect     Optional sub-rectangle of the texture
        """
        Image.__init__(self, texture, rect)
        FunctionalBase.__init__(self)
