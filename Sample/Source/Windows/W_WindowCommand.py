# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict, Optional, Union, Tuple
from Engine import Image, IntRect, System, Vector2f
from Engine.UI import ListView
from Engine.Utils import Math
from Engine.UI.Base import FunctionalBase
from Engine.UI.FunctionalUI import FPlainText
from .W_WindowSelectable import WindowSelectable


class WindowCommand(WindowSelectable):
    def __init__(
        self,
        rect: Union[IntRect, Tuple[Tuple[int, int], Tuple[int, int]]],
        commands: Dict[str, Dict[str, Any]] = {},
        rectWidth: Optional[int] = None,
        rectHeight: int = 32,
        fontIndex: int = 0,
        fontSize: float = 22,
        windowSkin: Optional[Image] = None,
        repeated: bool = False,
    ) -> None:
        super().__init__(rect, None, rectWidth, rectHeight, windowSkin, repeated)
        listView = ListView(self.content.getNoTranslationRect(), rectHeight, True, 2)
        if len(commands) > 0:
            for key, item in commands.items():
                child = FPlainText(System.getFonts()[fontIndex], item["text"], fontSize)
                if "callback" in item:
                    child.addConfirmCallback(item["callback"])
                self._applyItem(child)
                listView.addChild(child)
        self.setListView(listView)

    def onMouseWheelScrolled(self, kwargs: Dict[str, Any]):
        super().onMouseWheelScrolled(kwargs)

    def update(self, deltaTime: float) -> None:
        if self._mouseCursorSelect:
            for index, item in enumerate(self._listView.getChildren()):
                if isinstance(item, FunctionalBase):
                    if item.isHovered():
                        self.index = index
        super().update(deltaTime)
