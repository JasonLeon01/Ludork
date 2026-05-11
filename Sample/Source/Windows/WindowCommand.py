# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict, Optional, Union, Tuple
from Engine import Pair, Image, IntRect, UI
from Engine.UI import ListView
from Engine.UI.FunctionalUI import FPlainText
from .Base import WindowSelectable


class WindowCommand(WindowSelectable):
    r"""\brief A simple selectable command list window.

    Provides a vertical list of command items with callbacks
    triggered on confirmation.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        commands: Dict[str, Dict[str, Any]] = {},
        rectWidth: Optional[int] = None,
        rectHeight: int = 32,
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
        columns: int = 1,
    ) -> None:
        r"""\brief Construct a command window with the given commands.

        - \param rect The window rectangle.
        - \param commands Dictionary of command key to {text, callback}.
        - \param rectWidth Optional fixed width for the selection rectangle.
        - \param rectHeight Height of each command item.
        - \param windowSkin Optional window skin image.
        - \param repeated Whether the window skin is repeated.
        """
        super().__init__(rect, None, rectWidth, rectHeight, windowSkin, repeated)
        listView = ListView(self.content.getNoTranslationRect(), rectHeight, True, columns)
        if len(commands) > 0:
            for key, item in commands.items():
                child = FPlainText(UI.DefaultFont, item["text"], UI.DefaultFontSize)
                if "callback" in item:
                    child.addConfirmCallback(item["callback"])
                self._applyItem(child)
                listView.addChild(child)
        self.setListView(listView)
