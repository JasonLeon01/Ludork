# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Callable, Dict, Optional
from Engine import Input
from Global import Manager
from .WindowCommand import WindowCommand
from ..System import System as GameSystem


class WindowMenu(WindowCommand):
    r"""\brief In-game menu window with command items and cancel support.

    Provides a vertical command list (Items, Equipment, Save, Load, Return to Title)
    that opens/closes with the cancel key.
    """

    def __init__(
        self,
        commands: Dict[str, Dict[str, Any]],
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the menu window.

        - \param commands Dictionary of command key to {text, callback}.
        - \param onClose Optional callback invoked when the menu is closed.
        """
        super().__init__(((0, 0), (192, 192)), commands)
        self._onCloseCallback = onClose

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle cancel key to close the menu.

        - \param kwargs Event data.
        """
        if not self.getActive():
            return
        if self.getVisible():
            if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
                Manager.playSE(GameSystem.getCancelSE())
                self.close()
                if self._onCloseCallback is not None:
                    self._onCloseCallback()
                return
        return super().onKeyDown(kwargs)

    def open(self) -> None:
        r"""\brief Open the menu window."""
        self.setVisible(True)
        self.setActive(True)

    def close(self) -> None:
        r"""\brief Close the menu window."""
        self.setVisible(False)
        self.setActive(False)
