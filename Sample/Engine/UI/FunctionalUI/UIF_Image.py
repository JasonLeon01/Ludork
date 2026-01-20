# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional
from Engine import Texture, IntRect
from .. import Image
from ..Base import FunctionalBase


class FunctionalImage(Image, FunctionalBase):
    def __init__(self, texture: Texture, rect: Optional[IntRect] = None) -> None:
        Image.__init__(self, texture, rect)
        FunctionalBase.__init__(self)
