# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Optional
from Engine import FloatRect, Vector2f
from Engine.UI import Canvas, Image as UIImage
from Engine.UI.Base import FunctionalBase
from Global import Manager


_DEFAULT_WIDTH: int = 64
_LINE_IMAGE_WIDTH: int = 64
_LINE_HEIGHT: int = 4
_HANDLE_WIDTH: int = 2
_HANDLE_HEIGHT: int = 8
_LINE_IMAGE_NAME: str = "SliderLine.png"
_HANDLE_IMAGE_NAME: str = "SliderHandle.png"


class _SliderField(Canvas):
    r"""\brief Slider field rendered as a horizontal line and vertical handle."""

    def __init__(self, width: int, value: int, minValue: int, maxValue: int) -> None:
        r"""\brief Construct the slider field.

        - \param width     Slider line width in logical UI units
        - \param value     Initial integer value
        - \param minValue  Minimum integer value
        - \param maxValue  Maximum integer value
        """
        self._width = max(_HANDLE_WIDTH, int(width))
        self._minValue = int(minValue)
        self._maxValue = int(maxValue)
        if self._maxValue < self._minValue:
            self._minValue, self._maxValue = self._maxValue, self._minValue
        Canvas.__init__(self, ((0, 0), (self._width, _HANDLE_HEIGHT)))
        lineY = float(_HANDLE_HEIGHT - _LINE_HEIGHT) / 2.0
        self._line = UIImage(Manager.loadSystem(_LINE_IMAGE_NAME))
        self._line.setScale(Vector2f(float(self._width) / float(_LINE_IMAGE_WIDTH), 1.0))
        self._line.setPosition(Vector2f(0.0, lineY))
        self.addChild(self._line)
        self._handle = UIImage(Manager.loadSystem(_HANDLE_IMAGE_NAME))
        self.addChild(self._handle)
        self.setValue(value)

    def getChildren(self) -> list:
        r"""\brief Render this field as a single canvas texture.

        - \return  Empty list
        """
        return []

    def getLogicalSize(self) -> Vector2f:
        r"""\brief Get the field size in logical UI units.

        - \return  Width and height of the slider field
        """
        size = self.getSize()
        return Vector2f(float(size.x), float(size.y))

    def getHandlePosition(self) -> int:
        r"""\brief Get the handle x-position in logical UI units.

        - \return  Current handle x-position
        """
        return int(round(self._handle.getPosition().x))

    def setRange(self, minValue: int, maxValue: int, value: int) -> None:
        r"""\brief Update the integer range and current value.

        - \param minValue  Minimum integer value
        - \param maxValue  Maximum integer value
        - \param value     Current value to clamp into the new range
        """
        self._minValue = int(minValue)
        self._maxValue = int(maxValue)
        if self._maxValue < self._minValue:
            self._minValue, self._maxValue = self._maxValue, self._minValue
        self.setValue(value)

    def setValue(self, value: int) -> None:
        r"""\brief Move the handle to match an integer value.

        - \param value  Value to display
        """
        self._handle.setPosition(Vector2f(float(self._valueToPosition(value)), 0.0))
        self._buildRenderQueue()
        self.render()

    def update(self, deltaTime: float) -> None:
        r"""\brief Refresh and render the slider field canvas.

        - \param deltaTime  Elapsed time in seconds
        """
        for child in self._childrenList:
            if isinstance(child, FunctionalBase) and child.getVisible():
                child.update(deltaTime)
        self._buildRenderQueue()
        self.render()

    def _valueToPosition(self, value: int) -> int:
        if self._maxValue == self._minValue:
            return 0
        rate = (int(value) - self._minValue) / float(self._maxValue - self._minValue)
        rate = max(0.0, min(1.0, rate))
        return int(round(rate * float(self._width - _HANDLE_WIDTH)))


class Slider:
    r"""\brief Integer slider coordinator with a drawable field."""

    def __init__(
        self,
        value: int = 0,
        minValue: int = 0,
        maxValue: int = 100,
        width: int = _DEFAULT_WIDTH,
        onValueChanged: Optional[Callable[[int], None]] = None,
    ) -> None:
        r"""\brief Construct a slider coordinator.

        - \param value           Initial integer value
        - \param minValue        Minimum integer value
        - \param maxValue        Maximum integer value
        - \param width           Slider line width in logical UI units
        - \param onValueChanged  Callback invoked when the value changes
        """
        self._minValue = int(minValue)
        self._maxValue = int(maxValue)
        if self._maxValue < self._minValue:
            self._minValue, self._maxValue = self._maxValue, self._minValue
        self._value = self._clampValue(value)
        self._onValueChanged = onValueChanged
        self._field = _SliderField(width, self._value, self._minValue, self._maxValue)

    def getValue(self) -> int:
        r"""\brief Get the current integer value.

        - \return  Current slider value
        """
        return self._value

    def setValue(self, value: int) -> None:
        r"""\brief Set the current integer value.

        - \param value  New value clamped into the slider range
        """
        newValue = self._clampValue(value)
        if self._value == newValue:
            return
        self._value = newValue
        self._field.setValue(self._value)
        if self._onValueChanged is not None:
            self._onValueChanged(self._value)

    def setRange(self, minValue: int, maxValue: int) -> None:
        r"""\brief Set the integer value range.

        - \param minValue  Minimum integer value
        - \param maxValue  Maximum integer value
        """
        self._minValue = int(minValue)
        self._maxValue = int(maxValue)
        if self._maxValue < self._minValue:
            self._minValue, self._maxValue = self._maxValue, self._minValue
        self._value = self._clampValue(self._value)
        self._field.setRange(self._minValue, self._maxValue, self._value)

    def getRange(self) -> tuple[int, int]:
        r"""\brief Get the current integer range.

        - \return  Minimum and maximum values
        """
        return (self._minValue, self._maxValue)

    def setValueFromRatio(self, ratio: float) -> None:
        r"""\brief Set value from a 0-1 slider position.

        - \param ratio  Horizontal position ratio
        """
        ratio = max(0.0, min(1.0, float(ratio)))
        value = self._minValue + round(ratio * float(self._maxValue - self._minValue))
        self.setValue(value)

    def setValueFromBoundsPosition(self, bounds: FloatRect, position: Vector2f) -> None:
        r"""\brief Set value from an absolute mouse position and field bounds.

        - \param bounds    Absolute slider field bounds
        - \param position  Absolute mouse position
        """
        if bounds.size.x <= 0:
            return
        self.setValueFromRatio((position.x - bounds.position.x) / bounds.size.x)

    def adjust(self, delta: int) -> None:
        r"""\brief Adjust the current value by an integer delta.

        - \param delta  Value delta
        """
        self.setValue(self._value + int(delta))

    def getHandlePosition(self) -> int:
        r"""\brief Get the handle x-position in logical UI units.

        - \return  Current handle x-position
        """
        return self._field.getHandlePosition()

    def setOnValueChanged(self, onValueChanged: Optional[Callable[[int], None]]) -> None:
        r"""\brief Register a callback for value changes.

        - \param onValueChanged  Callback invoked with the new value
        """
        self._onValueChanged = onValueChanged

    def getField(self) -> _SliderField:
        r"""\brief Get the slider field widget.

        - \return  Slider field widget
        """
        return self._field

    def getSize(self) -> Vector2f:
        r"""\brief Get the slider field size.

        - \return  Size in logical UI units
        """
        return self._field.getLogicalSize()

    def _clampValue(self, value: int) -> int:
        return max(self._minValue, min(self._maxValue, int(round(value))))
