# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import List, Optional
from Engine import Image, Vector2f, UI, FloatRect
from Engine.UI import Canvas, PlainText, ListView
from Engine.UI.Base import FunctionalBase
from .DropBox import DropBox, _expandedOuterHeight


_ROW_HEIGHT: int = 32
_LABEL_X: int = 32
_LABEL_FONT_SIZE: int = 20


class ConfigSettingRow(Canvas, FunctionalBase):
    r"""\brief Single configuration row combining a label and a DropBox.

    Rendered as an off-screen canvas child of a ListView. Row height grows when
    the nested DropBox is expanded.
    """

    def __init__(
        self,
        labelText: str,
        items: List[str],
        rowWidth: int,
        dropboxWidth: int,
        windowSkin: Optional[Image],
        selectedIndex: int = 0,
    ) -> None:
        r"""\brief Construct a configuration setting row.

        - \param labelText      Localized label shown on the left
        - \param items          DropBox option labels
        - \param rowWidth       Total row width in logical UI units
        - \param dropboxWidth   DropBox field width
        - \param windowSkin     Windowskin shared with the parent window
        - \param selectedIndex  Initial DropBox selection index
        """
        maxHeight = _expandedOuterHeight(len(items))
        Canvas.__init__(self, ((0, 0), (rowWidth, maxHeight)))
        FunctionalBase.__init__(self)
        self._rowWidth = rowWidth
        self._label = PlainText(UI.DefaultFont, labelText, _LABEL_FONT_SIZE)
        labelBounds = self._label.getLocalBounds()
        labelY = (float(_ROW_HEIGHT) - labelBounds.size.y) / 2.0
        self._label.setPosition(Vector2f(float(_LABEL_X) - labelBounds.position.x, labelY))
        self._label.setOrigin(Vector2f(0.0, 0.0))
        self.addChild(self._label)
        self._dropBox = DropBox(
            items,
            selectedIndex,
            dropboxWidth,
            windowSkin,
            self._onDropBoxLayoutChanged,
        )
        dropX = float(rowWidth) / 2.0
        self._dropBox.getField().setPosition(Vector2f(dropX, 0.0))
        self._dropBox.getExpandedView().setPosition(Vector2f(dropX, 0.0))
        for widget in self._dropBox.getWidgets():
            self.addChild(widget)
        self.setOrigin(Vector2f(0.0, 0.0))

    def getDropBox(self) -> DropBox:
        r"""\brief Get the row DropBox coordinator.

        - \return  Nested DropBox instance
        """
        return self._dropBox

    def getChildren(self) -> list:
        r"""\brief Disable default canvas child traversal; this row renders as a unit.

        - \return  Empty list
        """
        return []

    def getSize(self) -> Vector2f:
        r"""\brief Get the current row size for ListView layout.

        - \return  Row width and height in logical UI units
        """
        return Vector2f(float(self._rowWidth), self._dropBox.getSize().y)

    def getLocalBounds(self) -> FloatRect:
        r"""\brief Get the visible row bounds instead of the backing canvas size.

        - \return  Local bounds matching the current row height
        """
        return FloatRect(Vector2f(0.0, 0.0), self.getSize())

    def update(self, deltaTime: float) -> None:
        r"""\brief Update nested widgets and render the row canvas.

        - \param deltaTime  Elapsed time in seconds
        """
        super().update(deltaTime)
        self.render()

    def _onDropBoxLayoutChanged(self) -> None:
        parent = self.getParent()
        if isinstance(parent, ListView):
            parent._positionsSettled = False
            parent.applyPositions()
