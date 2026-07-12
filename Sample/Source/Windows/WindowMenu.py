# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Dict, Optional
from Engine import Input
from Engine.UI.Base import Direction, FunctionalBase
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
        if Input.isActionTriggered(Input.getCancelKeys(), handled=False):
            if self._returnEquipSelectToSlot():
                Input.isActionTriggered(Input.getCancelKeys(), handled=True)
                return
            if self._returnSaveSlotToCommand():
                Input.isActionTriggered(Input.getCancelKeys(), handled=True)
                return
            if self._closeSubMenus():
                self.requestKeyboardFocus()
                Input.isActionTriggered(Input.getCancelKeys(), handled=True)
                return
            self._closeByCancel()
            Input.isActionTriggered(Input.getCancelKeys(), handled=True)
            return
        return super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        r"""\brief Handle right-click cancel to close the menu."""
        if kwargs["button"] == Input.Mouse.Button.Right:
            if self._returnEquipSelectToSlot():
                return True
            if self._returnSaveSlotToCommand():
                return True
            if self._closeSubMenus():
                self.requestKeyboardFocus()
                return True
            self._closeByCancel()
            return True
        return False

    def onDirectionalKey(self, direction: Direction) -> bool:
        r"""\brief Move menu cursor or jump to the currently opened submenu.

        - \param direction Navigation direction.

        - \return True if the direction was handled.
        """
        if direction == Direction.RIGHT:
            target = self._getCurrentSubMenuFocusTarget()
            if target is not None:
                target.requestKeyboardFocus()
                return True
        return super().onDirectionalKey(direction)

    def open(self) -> None:
        r"""\brief Open the menu window and disable player movement."""
        Manager.playSE(GameSystem.getDecisionSE())
        self._player.setMoveEnabled(False)
        self.setVisible(True)
        self.setActive(True)
        self.requestKeyboardFocus()

    def close(self) -> None:
        r"""\brief Close the menu window and restore player movement."""
        self._closeSubMenus()
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
        self._closeSubMenus(exceptName="item")
        self._windowItem.open()
        self._windowItem.requestKeyboardFocus()

    def _onItemClose(self) -> None:
        self.requestKeyboardFocus()

    def _onItemUsed(self) -> None:
        self.close()

    def _onMenuEquip(self, kwargs: Dict[str, Any] = {}) -> None:
        if self._windowEquipSlot is None or self._windowEquipSelect is None:
            return
        Manager.playSE(GameSystem.getDecisionSE())
        self._closeSubMenus(exceptName="equip")
        self._windowEquipSelect.open()
        self._windowEquipSlot.open()
        self._windowEquipSlot.requestKeyboardFocus()

    def _onEquipClose(self) -> None:
        self.requestKeyboardFocus()

    def _onEquipUsed(self) -> None:
        pass

    def _onMenuSave(self, kwargs: Dict[str, Any] = {}) -> None:
        if self._windowSaveLoad is None:
            return
        Manager.playSE(GameSystem.getDecisionSE())
        self._closeSubMenus(exceptName="save")
        self._windowSaveLoad.open()
        saveCommandWindow = self._windowSaveLoad.getCommandWindow()
        if saveCommandWindow is not None and saveCommandWindow.getActive():
            saveCommandWindow.requestKeyboardFocus()
        else:
            self._windowSaveLoad.getSlotWindow().requestKeyboardFocus()

    def _onSaveLoadClose(self) -> None:
        self.requestKeyboardFocus()

    def _onMenuExit(self, kwargs: Dict[str, Any] = {}) -> None:
        from Source.Scenes import Title

        GlobalSystem.setScene(Title())

    def _getCurrentSubMenuFocusTarget(self) -> Optional[FunctionalBase]:
        if self.index == 0 and self._windowItem.getVisible():
            return self._windowItem
        if self.index == 1 and self._windowEquipSlot is not None and self._windowEquipSlot.getVisible():
            return self._windowEquipSlot
        if self.index == 2 and self._windowSaveLoad is not None and self._windowSaveLoad.getVisible():
            saveCommandWindow = self._windowSaveLoad.getCommandWindow()
            if saveCommandWindow is not None and saveCommandWindow.getActive():
                return saveCommandWindow
            return self._windowSaveLoad.getSlotWindow()
        return None

    def _closeSubMenus(self, exceptName: str = "") -> bool:
        closed = False
        if exceptName != "item" and self._windowItem.getVisible():
            self._windowItem.close()
            closed = True
        if exceptName != "equip" and self._windowEquipSlot is not None and self._windowEquipSlot.getVisible():
            self._windowEquipSlot.close()
            if self._windowEquipSelect is not None:
                self._windowEquipSelect.close()
            closed = True
        if exceptName != "equip" and self._windowEquipSelect is not None and self._windowEquipSelect.getVisible():
            self._windowEquipSelect.close()
            closed = True
        if exceptName != "save" and self._windowSaveLoad is not None and self._windowSaveLoad.getVisible():
            self._windowSaveLoad.close()
            closed = True
        return closed

    def _returnEquipSelectToSlot(self) -> bool:
        if self._windowEquipSelect is None:
            return False
        if not self._windowEquipSelect.getVisible():
            return False
        if not (self._windowEquipSelect.getActive() or self._windowEquipSelect.getFocused()):
            return False
        self._windowEquipSelect.returnToSlotWindow()
        return True

    def _returnSaveSlotToCommand(self) -> bool:
        if self._windowSaveLoad is None or not self._windowSaveLoad.getVisible():
            return False
        slotWindow = self._windowSaveLoad.getSlotWindow()
        if not (slotWindow.getActive() or slotWindow.getFocused()):
            return False
        return self._windowSaveLoad.returnToCommandWindow()
