# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Optional
from Engine import Image, IntRect, Vector2i, Vector2f, UI, Color
from Engine.UI import Canvas, PlainText, Rect
from Engine.UI.Base import FunctionalBase


_BOX_SIZE: int = 32
_FONT_SIZE: int = 20
_CHECKED_MARK: str = "√"


class _CheckBoxField(Canvas):
    r"""\brief Checkbox field rendered with a windowskin frame and centred mark."""

    def __init__(
        self,
        checked: bool,
        size: int,
        windowSkin: Image,
    ) -> None:
        r"""\brief Construct a checkbox field.

        - \param checked     Initial checked state
        - \param size        Checkbox width and height in logical UI units
        - \param windowSkin  Windowskin image
        """
        Canvas.__init__(self, ((0, 0), (size, size)))
        self._size = size
        self._frame = Rect(IntRect(Vector2i(0, 0), Vector2i(size, size)), windowSkin)
        self.addChild(self._frame)
        self._markText = PlainText(UI.DefaultFont, "", _FONT_SIZE)
        self.addChild(self._markText)
        self.setChecked(checked)
        self._buildRenderQueue()
        self.render()

    def getChildren(self) -> list:
        r"""\brief Render this field as a single canvas texture.

        - \return  Empty list
        """
        return []

    def setChecked(self, checked: bool) -> None:
        r"""\brief Update the visual checked state.

        - \param checked  True to show the checked mark
        """
        self._markText.setString(_CHECKED_MARK if checked else "")
        self._markText.setColour(Color.Green if checked else Color.White)
        self._markText.setPosition(self._getMarkPosition())

    def getLogicalSize(self) -> Vector2f:
        r"""\brief Get the field size in logical UI units.

        - \return  Width and height of the checkbox field
        """
        size = self.getSize()
        return Vector2f(float(size.x), float(size.y))

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Refresh and render the checkbox field canvas.

        - \param deltaTime  Elapsed time in seconds
        """
        self._buildRenderQueue()
        self.render()

    def _getMarkPosition(self) -> Vector2f:
        bounds = self._markText.getLocalBounds()
        posX = (float(self._size) - bounds.size.x) / 2.0 - bounds.position.x
        posY = (float(self._size) - bounds.size.y) / 2.0
        return Vector2f(posX, posY)


class CheckBox:
    r"""\brief Checkbox coordinator managing the checked state of a field widget."""

    def __init__(
        self,
        checked: bool = False,
        size: int = _BOX_SIZE,
        windowSkin: Optional[Image] = None,
        onCheckedChanged: Optional[Callable[[bool], None]] = None,
    ) -> None:
        r"""\brief Construct a checkbox coordinator.

        - \param checked           Initial checked state
        - \param size              Checkbox width and height in logical UI units
        - \param windowSkin        Windowskin image
        - \param onCheckedChanged  Callback invoked when the checked state changes
        """
        from Global import Manager

        if windowSkin is None:
            windowSkin = Manager.loadSystem(UI.DefaultWindowskinName, smooth=True).copyToImage()
        self._checked = checked
        self._onCheckedChanged = onCheckedChanged
        self._field = _CheckBoxField(self._checked, size, windowSkin)

    def isChecked(self) -> bool:
        r"""\brief Check whether the checkbox is currently checked.

        - \return  True when checked
        """
        return self._checked

    def setChecked(self, checked: bool) -> None:
        r"""\brief Set the checked state and refresh the field.

        - \param checked  New checked state
        """
        if self._checked == checked:
            return
        self._checked = checked
        self._field.setChecked(self._checked)
        if self._onCheckedChanged is not None:
            self._onCheckedChanged(self._checked)

    def toggle(self) -> None:
        r"""\brief Toggle the checked state."""
        self.setChecked(not self._checked)

    def setOnCheckedChanged(self, onCheckedChanged: Optional[Callable[[bool], None]]) -> None:
        r"""\brief Register a callback for checked-state changes.

        - \param onCheckedChanged  Callback invoked with the new checked flag
        """
        self._onCheckedChanged = onCheckedChanged

    def getField(self) -> _CheckBoxField:
        r"""\brief Get the checkbox field widget.

        - \return  Checkbox field widget
        """
        return self._field

    def getSize(self) -> Vector2f:
        r"""\brief Get the checkbox field size.

        - \return  Size in logical UI units
        """
        return self._field.getLogicalSize()
