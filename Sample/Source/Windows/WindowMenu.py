# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict
from Engine import Input
from Global import Manager, System as GlobalSystem
from .WindowCommand import WindowCommand
from ..System import System as GameSystem


class WindowMenu(WindowCommand):
    r"""\brief In-game menu window that manages commands, open/close triggers, and sub-windows.

    Owns the full menu lifecycle: detects the open trigger, defines built-in commands
    (Items, Equipment, Save, Load, Return to Title), delegates to WindowItem, and
    re-enables player movement on close.
    """

    def __init__(
        self,
        player,
        windowItem,
        messageWindow,
        windowEquipSlot=None,
        windowEquipSelect=None,
        windowSaveLoad=None,
    ) -> None:
        r"""\brief Construct the menu window and wire up sub-window callbacks.

        - \param player The player actor; movement is disabled while the menu is open.
        - \param windowItem The item sub-window to open when Items is selected.
        - \param messageWindow The message window; the menu will not open during dialogue.
        - \param windowEquipSlot The equipped-slot sub-window.
        - \param windowEquipSelect The available-equip sub-window.
        - \param windowSaveLoad The integrated save/load sub-window.
        """
        commands = {
            "Items": {"text": LOC("MENU_ITEM"), "callback": lambda obj, kwargs: self._onMenuItem(kwargs)},
            "Equipment": {"text": LOC("MENU_EQUIP"), "callback": lambda obj, kwargs: self._onMenuEquip(kwargs)},
            "Save": {"text": LOC("MENU_SAVE_FILE"), "callback": lambda obj, kwargs: self._onMenuSave(kwargs)},
            "ReturnTitle": {"text": LOC("MENU_EXIT"), "callback": lambda obj, kwargs: self._onMenuExit(kwargs)},
        }
        super().__init__(((0, 0), (192, 160)), commands)
        self._player = player
        self._windowItem = windowItem
        self._messageWindow = messageWindow
        self._windowEquipSlot = windowEquipSlot
        self._windowEquipSelect = windowEquipSelect
        self._windowSaveLoad = windowSaveLoad
        self._moveRestoreGuard = lambda: True
        self._windowItem._onCloseCallback = self._onItemClose
        self._windowItem._onUseCallback = self._onItemUsed
        if self._windowEquipSlot is not None:
            self._windowEquipSlot._onCloseCallback = self._onEquipClose
        if self._windowEquipSelect is not None:
            self._windowEquipSelect._onEquipCallback = self._onEquipUsed

    def setMoveRestoreGuard(self, guard) -> None:
        r"""\brief Set a predicate that decides whether close restores player movement.

        - \param guard Callable returning True when movement may be restored.
        """
        self._moveRestoreGuard = guard

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
        if self._moveRestoreGuard():
            self._player.setMoveEnabled(True)

    def isBlocking(self) -> bool:
        r"""\brief Return True when the menu or its sub-windows are blocking map input."""
        equipVisible = self._windowEquipSlot is not None and self._windowEquipSlot.getVisible()
        saveLoadVisible = self._windowSaveLoad is not None and self._windowSaveLoad.getVisible()
        return self.getVisible() or self._windowItem.getVisible() or equipVisible or saveLoadVisible

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
        if self._windowEquipSlot is None or self._windowEquipSelect is None:
            return
        Manager.playSE(GameSystem.getDecisionSE())
        self.setActive(False)
        self._windowEquipSelect.open()
        self._windowEquipSlot.open()

    def _onEquipClose(self) -> None:
        self.setActive(True)

    def _onEquipUsed(self) -> None:
        pass

    def _onMenuSave(self, kwargs: Dict[str, Any] = {}) -> None:
        if self._windowSaveLoad is None:
            return
        Manager.playSE(GameSystem.getDecisionSE())
        self.setActive(False)
        self._windowSaveLoad.open()

    def _onSaveLoadClose(self) -> None:
        self.setActive(True)

    def _onMenuExit(self, kwargs: Dict[str, Any] = {}) -> None:
        from Source.Scenes import Title

        GlobalSystem.setScene(Title())
