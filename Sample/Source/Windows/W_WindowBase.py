# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, Tuple
from Engine import Pair, Image, IntRect
from Engine.UI import Canvas, Window


class WindowBase(Canvas):
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
    ) -> None:
        super().__init__(rect)
        self._window = Window(self.getNoTranslationRect(), windowSkin, repeated)
        self.content = Canvas(self.getContentRect())
        self.addChild(self._window)
        self.addChild(self.content)
