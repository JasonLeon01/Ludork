# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict
from Engine import Input
from Engine.UI.Base import FunctionalBase
from Global import Manager, System as GlobalSystem
from .WindowCommand import WindowCommand
from ..System import System as GameSystem


class WindowMenu(WindowCommand):
    r"""\brief In-game menu window that manages commands, open/close triggers, and sub-windows.

    Owns the full menu lifecycle: detects the open trigger, defines built-in commands
    (Items, Equipment, Save, Load, Return to Title), delegates to WindowItem, and
    re-enables player movement on close.
    """

    def __init__(self, player, windowItem, messageWindow) -> None:
        r"""\brief Construct the menu window and wire up sub-window callbacks.

        - \param player The player actor; movement is disabled while the menu is open.
        - \param windowItem The item sub-window to open when Items is selected.
        - \param messageWindow The message window; the menu will not open during dialogue.
        """
        commands = {
            "Items": {"text": LOC("MENU_ITEM"), "callback": lambda obj, kwargs: self._onMenuItem(kwargs)},
            "Equipment": {"text": LOC("MENU_EQUIP"), "callback": lambda obj, kwargs: self._onMenuEquip(kwargs)},
            "Save": {"text": LOC("MENU_SAVE"), "callback": lambda obj, kwargs: self._onMenuSave(kwargs)},
            "Load": {"text": LOC("MENU_LOAD"), "callback": lambda obj, kwargs: self._onMenuLoad(kwargs)},
            "ReturnTitle": {"text": LOC("MENU_EXIT"), "callback": lambda obj, kwargs: self._onMenuExit(kwargs)},
        }
        super().__init__(((0, 0), (192, 192)), commands)
        self._player = player
        self._windowItem = windowItem
        self._messageWindow = messageWindow
        self._windowItem._onCloseCallback = self._onItemClose
        self._windowItem._onUseCallback = self._onItemUsed

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle cancel key to close the menu.

        - \param kwargs Event data.
        """
        if not self.getActive():
            return
        if self.getVisible():
            if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
                self._closeByCancel()
                return
        return super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Handle right-click cancel to close the menu."""
        if not self.getActive() or not self.getVisible():
            return
        if Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True):
            self._closeByCancel()

    def open(self) -> None:
        r"""\brief Open the menu window and disable player movement."""
        Manager.playSE(GameSystem.getDecisionSE())
        self._player.setMoveEnabled(False)
        self.setVisible(True)
        self.setActive(True)

    def close(self) -> None:
        r"""\brief Close the menu window and restore player movement."""
        self.setVisible(False)
        self.setActive(False)
        self._player.setMoveEnabled(True)

    def isBlocking(self) -> bool:
        r"""\brief Return True when the menu or its sub-windows are blocking map input."""
        return self.getVisible() or self._windowItem.getVisible()

    def _closeByCancel(self) -> None:
        Manager.playSE(GameSystem.getCancelSE())
        self.close()

    def _onMenuItem(self, kwargs: Dict[str, Any] = {}) -> None:
        Manager.playSE(GameSystem.getDecisionSE())
        self.setActive(False)
        self._windowItem.open()

    def _onItemClose(self) -> None:
        self.setActive(True)

    def _onItemUsed(self) -> None:
        self.close()

    def _onMenuEquip(self, kwargs: Dict[str, Any] = {}) -> None:
        pass

    def _onMenuSave(self, kwargs: Dict[str, Any] = {}) -> None:
        pass

    def _onMenuLoad(self, kwargs: Dict[str, Any] = {}) -> None:
        pass

    def _onMenuExit(self, kwargs: Dict[str, Any] = {}) -> None:
        from Source.Scenes import Title

        GlobalSystem.setScene(Title())
