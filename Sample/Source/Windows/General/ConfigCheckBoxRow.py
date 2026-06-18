# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Optional
from Engine import Image, Vector2f, UI, FloatRect
from Engine.UI import Canvas, PlainText
from Engine.UI.Base import FunctionalBase
from .CheckBox import CheckBox


_ROW_HEIGHT: int = 32
_LABEL_X: int = 32
_LABEL_FONT_SIZE: int = 20


class ConfigCheckBoxRow(Canvas, FunctionalBase):
    r"""\brief Single configuration row combining a label and a CheckBox."""

    def __init__(
        self,
        labelText: str,
        rowWidth: int,
        checkBoxSize: int,
        windowSkin: Optional[Image],
        checked: bool = False,
        onCheckedChanged: Optional[Callable[[bool], None]] = None,
    ) -> None:
        r"""\brief Construct a configuration checkbox row.

        - \param labelText         Localized label shown on the left
        - \param rowWidth          Total row width in logical UI units
        - \param checkBoxSize      Checkbox width and height
        - \param windowSkin        Windowskin shared with the parent window
        - \param checked           Initial checked state
        - \param onCheckedChanged  Callback invoked when the checked state changes
        """
        Canvas.__init__(self, ((0, 0), (rowWidth, _ROW_HEIGHT)))
        FunctionalBase.__init__(self)
        self._rowWidth = rowWidth
        self._label = PlainText(UI.DefaultFont, labelText, _LABEL_FONT_SIZE)
        labelBounds = self._label.getLocalBounds()
        labelY = (float(_ROW_HEIGHT) - labelBounds.size.y) / 2.0
        self._label.setPosition(Vector2f(float(_LABEL_X) - labelBounds.position.x, labelY))
        self._label.setOrigin(Vector2f(0.0, 0.0))
        self.addChild(self._label)
        self._checkBox = CheckBox(checked, checkBoxSize, windowSkin, onCheckedChanged)
        checkBoxY = (float(_ROW_HEIGHT) - float(checkBoxSize)) / 2.0
        self._checkBox.getField().setPosition(Vector2f(float(rowWidth) / 2.0, checkBoxY))
        self.addChild(self._checkBox.getField())
        self.addConfirmCallback(self._onConfirmToggle)
        self.setOrigin(Vector2f(0.0, 0.0))

    def getCheckBox(self) -> CheckBox:
        r"""\brief Get the row CheckBox coordinator.

        - \return  Nested CheckBox instance
        """
        return self._checkBox

    def getChildren(self) -> list:
        r"""\brief Disable default canvas child traversal; this row renders as a unit.

        - \return  Empty list
        """
        return []

    def getSize(self) -> Vector2f:
        r"""\brief Get the row size for ListView layout.

        - \return  Row width and height in logical UI units
        """
        return Vector2f(float(self._rowWidth), float(_ROW_HEIGHT))

    def getLocalBounds(self) -> FloatRect:
        r"""\brief Get the visible row bounds matching the checkbox row height.

        - \return  Local bounds for hit-testing and pointer focus
        """
        return FloatRect(Vector2f(0.0, 0.0), self.getSize())

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update nested widgets and render the row canvas.

        - \param deltaTime  Elapsed time in seconds
        """
        self._buildRenderQueue()
        self.render()

    def _onConfirmToggle(self, kwargs: dict) -> None:
        self._checkBox.toggle()
