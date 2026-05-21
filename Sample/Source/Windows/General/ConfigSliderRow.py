# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Optional
from Engine import Vector2f, UI, FloatRect, Input
from Engine.UI import Canvas, PlainText
from Engine.UI.Base import FunctionalBase
from Engine.Utils import Math
from .Slider import Slider


_ROW_HEIGHT: int = 32
_LABEL_X: int = 32
_LABEL_FONT_SIZE: int = 20
_VALUE_PAD_X: int = 16


class ConfigSliderRow(Canvas, FunctionalBase):
    r"""\brief Single configuration row combining a label and a Slider."""

    def __init__(
        self,
        labelText: str,
        rowWidth: int,
        sliderWidth: int,
        value: int = 0,
        onValueChanged: Optional[Callable[[int], None]] = None,
    ) -> None:
        r"""\brief Construct a configuration slider row.

        - \param labelText       Localized label shown on the left
        - \param rowWidth        Total row width in logical UI units
        - \param sliderWidth     Slider line width
        - \param value           Initial integer value
        - \param onValueChanged  Callback invoked when the slider value changes
        """
        Canvas.__init__(self, ((0, 0), (rowWidth, _ROW_HEIGHT)))
        FunctionalBase.__init__(self)
        self._rowWidth = rowWidth
        self._dragging = False
        self._label = PlainText(UI.DefaultFont, labelText, _LABEL_FONT_SIZE)
        labelBounds = self._label.getLocalBounds()
        labelY = (float(_ROW_HEIGHT) - labelBounds.size.y) / 2.0
        self._label.setPosition(Vector2f(float(_LABEL_X) - labelBounds.position.x, labelY))
        self._label.setOrigin(Vector2f(0.0, 0.0))
        self.addChild(self._label)
        self._onValueChanged = onValueChanged
        self._slider = Slider(
            value,
            0,
            100,
            sliderWidth,
            self._onSliderValueChanged,
        )
        sliderY = (float(_ROW_HEIGHT) - self._slider.getSize().y) / 2.0
        sliderX = float(rowWidth) / 2.0
        self._slider.getField().setPosition(Vector2f(sliderX, sliderY))
        self.addChild(self._slider.getField())
        self._valueText = PlainText(UI.DefaultFont, "", _LABEL_FONT_SIZE)
        self.addChild(self._valueText)
        self._refreshValueText()
        self.setOrigin(Vector2f(0.0, 0.0))

    def getSlider(self) -> Slider:
        r"""\brief Get the row Slider coordinator.

        - \return  Nested Slider instance
        """
        return self._slider

    def adjust(self, delta: int) -> None:
        r"""\brief Adjust the nested slider value.

        - \param delta  Integer value delta
        """
        self._slider.adjust(delta)

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
        r"""\brief Get the visible row bounds matching the slider row height.

        - \return  Local bounds for hit-testing and pointer focus
        """
        return FloatRect(Vector2f(0.0, 0.0), self.getSize())

    def update(self, deltaTime: float) -> None:
        r"""\brief Update nested widgets, handle drag input, and render the row canvas.

        - \param deltaTime  Elapsed time in seconds
        """
        super().update(deltaTime)
        self._updateMouseDrag()
        self.render()

    def _onSliderValueChanged(self, value: int) -> None:
        self._refreshValueText()
        if self._onValueChanged is not None:
            self._onValueChanged(value)

    def _refreshValueText(self) -> None:
        self._valueText.setString(str(self._slider.getValue()))
        bounds = self._valueText.getLocalBounds()
        sliderField = self._slider.getField()
        sliderPos = sliderField.getPosition()
        sliderSize = self._slider.getSize()
        valueX = sliderPos.x + sliderSize.x + float(_VALUE_PAD_X) - bounds.position.x
        valueY = (float(_ROW_HEIGHT) - bounds.size.y) / 2.0
        self._valueText.setPosition(Vector2f(valueX, valueY))
        self._valueText.setOrigin(Vector2f(0.0, 0.0))

    def _updateMouseDrag(self) -> None:
        if not self.getActive() or not Input.isMouseInputMode():
            self._dragging = False
            return
        if not Input.isMouseButtonDown(Input.Mouse.Button.Left):
            self._dragging = False
            return
        mousePos = Math.ToVector2f(Input.getMousePosition())
        bounds: FloatRect = self._slider.getField().getAbsoluteBounds()
        if not self._dragging and not bounds.contains(mousePos):
            return
        self._dragging = True
        self._slider.setValueFromBoundsPosition(bounds, mousePos)
