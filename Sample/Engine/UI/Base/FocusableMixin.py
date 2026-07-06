# -*- encoding: utf-8 -*-

from __future__ import annotations
from enum import Enum
from typing import Optional


class Direction(Enum):
    r"""\brief Direction used by keyboard and gamepad focus navigation."""

    UP = "up"
    DOWN = "down"
    LEFT = "left"
    RIGHT = "right"


class FocusableMixin:
    r"""\brief Shared state for controls that can receive keyboard focus."""

    def __init__(self) -> None:
        r"""\brief Construct focusable state with focus disabled by default."""
        self._canReceiveFocus: bool = False
        self._isFocused: bool = False
        self._focusGroup: Optional[object] = None

    def setCanReceiveFocus(self, canReceiveFocus: bool) -> None:
        r"""\brief Set whether this control may receive keyboard focus.

        - \param canReceiveFocus True if the control can become focused.
        """
        self._canReceiveFocus = canReceiveFocus

    def getCanReceiveFocus(self) -> bool:
        r"""\brief Return whether this control is configured as focusable.

        - \return True if focus is enabled for this control.
        """
        return self._canReceiveFocus

    def getFocused(self) -> bool:
        r"""\brief Return whether this control currently owns keyboard focus.

        - \return True if the control is focused.
        """
        return self._isFocused

    def setFocused(self, focused: bool) -> None:
        r"""\brief Set the focused state and run focus callbacks.

        - \param focused True when focus is gained, False when focus is lost.
        """
        if self._isFocused == focused:
            return
        self._isFocused = focused
        if focused:
            self.onFocusGained()
        else:
            self.onFocusLost()

    def setFocusGroup(self, focusGroup: Optional[object]) -> None:
        r"""\brief Set the focus group that owns this control.

        - \param focusGroup Focus group object or None.
        """
        self._focusGroup = focusGroup

    def getFocusGroup(self) -> Optional[object]:
        r"""\brief Return the focus group that owns this control.

        - \return Focus group object or None.
        """
        return self._focusGroup

    def onFocusGained(self) -> None:
        r"""\brief Called when this control gains keyboard focus."""
        pass

    def onFocusLost(self) -> None:
        r"""\brief Called when this control loses keyboard focus."""
        pass
