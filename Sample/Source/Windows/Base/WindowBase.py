# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Optional, Union, Tuple
from Engine import Pair, Image, IntRect, UI
from Engine.UI import Canvas, Window
from Global import Manager


class WindowBase(Canvas):
    r"""\brief Base class for all game windows.

    Provides a window skin, content area, and nested canvas hierarchy.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
    ) -> None:
        r"""\brief Construct a window with a skin and content area.

        - \param rect The window rectangle.
        - \param windowSkin Optional window skin image; defaults to the system windowskin.
        - \param repeated Whether the window skin is repeated.
        """
        super().__init__(rect)
        if windowSkin is None:
            windowSkin = Manager.loadSystem(UI.DefaultWindowskinName, smooth=True).copyToImage()
        self._windowSkin = windowSkin
        self._window = Window(self.getNoTranslationRect(), windowSkin, repeated)
        self.content = Canvas(self.getContentRect())
        self.addChild(self._window)
        self.addChild(self.content)
