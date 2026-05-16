# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Callable, Dict, List, Optional, Union, Tuple
import Engine
from Engine import (
    Pair,
    Vector2i,
    Vector2u,
    IntRect,
    Texture,
    Vector2f,
    Input,
    UI,
    Text,
)
from Engine.UI import Canvas, ListView, PlainText, Rect
from Engine.UI.Base import FunctionalBase
from Engine.UI.FunctionalUI import FImage, FPlainText
from Engine.Utils import Math
from Global import Manager
from .Base import WindowSelectable
from ..System import System as GameSystem
from .. import Data

_SLOT_ROW_HEIGHT = 32
_SLOT_FONT_SIZE = 14
_SLOT_TEXT_PAD = 32
SLOT_Y_OFFSET = 8


class _SlotCell(Canvas, FunctionalBase):
    r"""\brief Equipped-slot row: optional icon on the left, name right-aligned."""

    def __init__(self, width: int, label: str, iconTexture: Optional[Texture]) -> None:
        r"""\brief Construct a slot row cell.

        - \param width Cell width in logical UI units.
        - \param label Display text for the slot row.
        - \param iconTexture Equipped item icon, or None when unequipped.
        """
        Canvas.__init__(self, ((0, 0), (width, _SLOT_ROW_HEIGHT)))
        FunctionalBase.__init__(self)
        if iconTexture is not None:
            self.addChild(FImage(iconTexture))
        text = FPlainText(UI.DefaultFont, label, _SLOT_FONT_SIZE)
        bounds = text.getLocalBounds()
        text.setLineAlignment(Text.LineAlignment.Right)
        text.setPosition(Vector2f(width - _SLOT_TEXT_PAD, SLOT_Y_OFFSET))
        self.addChild(text)

    def getChildren(self):
        return []

    def update(self, deltaTime: float) -> None:
        r"""\brief Update the cell and render to internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        super().update(deltaTime)
        self.render()


class _UnequipCell(Canvas, FunctionalBase):
    r"""\brief Empty unequip action cell."""

    def __init__(self) -> None:
        Canvas.__init__(self, ((0, 0), (32, 32)))
        FunctionalBase.__init__(self)

    def getChildren(self):
        return []

    def update(self, deltaTime: float) -> None:
        r"""\brief Update the cell and render to internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        super().update(deltaTime)
        self.render()


class _EquipCell(Canvas, FunctionalBase):
    r"""\brief Single equip cell displaying icon and count."""

    def __init__(self, iconTexture: Optional[Texture], count: int) -> None:
        r"""\brief Construct an equip cell.

        - \param iconTexture The loaded equip icon texture, or None if no icon available.
        - \param count The quantity of this equip in inventory.
        """
        Canvas.__init__(self, ((0, 0), (32, 32)))
        FunctionalBase.__init__(self)
        if iconTexture is not None:
            self.addChild(FImage(iconTexture))
        if count > 1:
            text = FPlainText(UI.DefaultFont, str(count), 12)
            text.setLineAlignment(Text.LineAlignment.Right)
            bounds = text.getLocalBounds()
            text.setPosition(
                Vector2f(32 - bounds.size.x - bounds.position.x - 1, 32 - bounds.size.y - bounds.position.y - 1)
            )
            self.addChild(text)

    def getChildren(self):
        return []

    def update(self, deltaTime: float) -> None:
        r"""\brief Update the cell and render to internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        super().update(deltaTime)
        self.render()


def _loadEquipIcon(iconPath: str) -> Optional[Texture]:
    r"""\brief Load an equip icon texture from the icon file path.

    - \param iconPath The icon file path from GeneralData.
    - \return Loaded Texture, or None if loading failed.
    """
    if not iconPath:
        return None
    try:
        if "/" in iconPath or "\\" in iconPath:
            parts = iconPath.replace("\\", "/").split("/")
            if len(parts) >= 2:
                subfolder = "/".join(parts[:-1])
                filename = parts[-1]
                return Manager.loadTexture(subfolder, filename)
        return Manager.loadTexture("Characters/items", iconPath)
    except Exception:
        return None


def _wrapDesc(text: str, maxWidth: float) -> str:
    from Engine import Scale

    charSize = int(14 * Scale)
    maxW = maxWidth * Scale
    font = UI.DefaultFont

    def adv(ch: str) -> float:
        return font.getGlyph(ch, charSize, False).advance

    def wrap_para(para: str) -> str:
        lines = []
        line = ""
        line_w = 0.0
        for word in para.split(" "):
            word_w = sum(adv(ch) for ch in word) if word else 0.0
            sep_w = adv(" ") if line else 0.0
            if line_w + sep_w + word_w <= maxW:
                line += (" " if line else "") + word
                line_w += sep_w + word_w
            else:
                if line:
                    lines.append(line)
                    line = ""
                    line_w = 0.0
                if word_w <= maxW:
                    line = word
                    line_w = word_w
                else:
                    for ch in word:
                        ch_w = adv(ch)
                        if line and line_w + ch_w > maxW:
                            lines.append(line)
                            line = ""
                            line_w = 0.0
                        line += ch
                        line_w += ch_w
        lines.append(line)
        return "\n".join(lines)

    return "\n".join(wrap_para(p) for p in text.split("\n"))


class WindowEquipSlot(WindowSelectable):
    r"""\brief Equipped-slot list window ordered by class slot keys.

    Shows currently equipped item names per slot, or unequipped placeholder text.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        player,
        windowEquipSelect: Optional["WindowEquipSelect"] = None,
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the equipped-slot window.

        - \param rect The window rectangle.
        - \param player The player instance.
        - \param windowEquipSelect The available-equip window to refresh on slot change.
        - \param onClose Optional callback invoked when the window is closed.
        """
        super().__init__(rect, None, None, _SLOT_ROW_HEIGHT)
        self._onCloseCallback = onClose
        self._player = player
        self._windowEquipSelect = windowEquipSelect
        self._slotKeys: List[str] = []
        self._lastSlotIndex: Optional[int] = None
        self._rectWidth = self._inRect.size.x - 32
        self._refreshSlots()
        self.setActive(False)
        self.setVisible(False)

    def setEquipSelectWindow(self, windowEquipSelect: "WindowEquipSelect") -> None:
        r"""\brief Set the available-equip window reference.

        - \param windowEquipSelect The available-equip window.
        """
        self._windowEquipSelect = windowEquipSelect

    def _getSlotCellData(self, slotKey: str) -> Tuple[Optional[Texture], str]:
        equipID = self._player.getEquipInfo(slotKey)
        if not equipID:
            return None, LOC("EQUIP_UNEQUIPPED")
        equipMembers = Data.getGeneralData("Equip").get("members", {})
        member = equipMembers.get(equipID, {})
        name = member.get("name", "")
        label = name.format(**LOC_D()) if name else equipID
        iconTex = _loadEquipIcon(member.get("icon", ""))
        return iconTex, label

    def _refreshSlots(self) -> None:
        r"""\brief Rebuild the slot list from the player's class slot order."""
        savedSlotKey = self._getCurrentSlotKey()
        classSlots = Data.getGeneralData("Class").get("members", {}).get(self._player.CLASS, {}).get("slot", {})
        self._slotKeys = list(classSlots.keys())
        cellWidth = self._inRect.size.x - 32
        self._rectWidth = cellWidth
        contentHeight = self.content.getSize().y
        listView = ListView(
            IntRect(Vector2i(0, 0), Vector2i(cellWidth + 32, contentHeight)),
            _SLOT_ROW_HEIGHT,
            True,
            1,
        )
        for slotKey in self._slotKeys:
            iconTex, label = self._getSlotCellData(slotKey)
            child = _SlotCell(cellWidth, label, iconTex)
            child.addConfirmCallback(lambda obj, kwargs: self._focusSelectWindow())
            self._applyItem(child)
            listView.addChild(child)
        self.setListView(listView)
        if savedSlotKey and savedSlotKey in self._slotKeys:
            self.index = self._slotKeys.index(savedSlotKey)
        else:
            self.index = 0 if len(self._slotKeys) > 0 else None
        self._lastSlotIndex = self.index
        self._resetSelectionRect()
        self._redrawIfVisible()

    def _resetSelectionRect(self) -> None:
        r"""\brief Recreate the selection rectangle to avoid ghosting after list rebuild."""
        if self._rect.getParent() is not None:
            self.content.removeChild(self._rect)
        pos = self._getRectPosition()
        if pos is None:
            self._rect.setVisible(False)
            return
        self._rect = Rect(
            IntRect(Math.ToVector2i(pos), Vector2i(self._rectWidth, self._rectHeight)),
            self._windowSkin,
        )
        self._rect.setVisible(True)
        self.content.addChild(self._rect)

    def _redrawIfVisible(self) -> None:
        r"""\brief Force redraw while visible even when inactive."""
        if not self.getVisible():
            return
        wasActive = self.getActive()
        if not wasActive:
            self.setActive(True)
        self.update(0.0)
        self.render()
        if not wasActive:
            self.setActive(False)

    def _getCurrentSlotKey(self) -> Optional[str]:
        if self.index is None or self.index >= len(self._slotKeys):
            return None
        return self._slotKeys[self.index]

    def _notifySlotChanged(self) -> None:
        slotKey = self._getCurrentSlotKey()
        if slotKey is not None and self._windowEquipSelect is not None and self._windowEquipSelect.getVisible():
            self._windowEquipSelect.refreshForSlot(slotKey)

    def update(self, deltaTime: float) -> None:
        r"""\brief Update slot window and notify slot change on index change.

        - \param deltaTime Elapsed time in seconds.
        """
        if self._rect.getParent() is not None:
            self.content.removeChild(self._rect)
        super().update(deltaTime)
        if self._lastSlotIndex != self.index:
            self._lastSlotIndex = self.index
            self._notifySlotChanged()

    def _focusSelectWindow(self) -> None:
        if self._windowEquipSelect is None:
            return
        Manager.playSE(GameSystem.getDecisionSE())
        slotKey = self._getCurrentSlotKey()
        if slotKey is not None:
            self._windowEquipSelect.refreshForSlot(slotKey)
        self.setActive(False)
        self._windowEquipSelect.setVisible(True)
        self._windowEquipSelect.setActive(True)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle cancel, confirm, and focus-switch keys.

        - \param kwargs Event data.
        """
        if not self.getActive():
            return
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._closeByCancel()
            return
        if Input.isActionTriggered(Input.getConfirmKeys(), handled=True):
            self._focusSelectWindow()
            return
        if Input.isActionTriggered(Input.getRightKeys(), handled=True):
            self._focusSelectWindow()
            return
        return super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Handle mouse cancel to close the slot window."""
        if not self.getActive() or not self.getVisible():
            return
        if Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True):
            self._closeByCancel()

    def open(self) -> None:
        r"""\brief Open the slot window, refreshing slot list first."""
        self._refreshSlots()
        self.setVisible(True)
        self.setActive(True)

    def close(self) -> None:
        r"""\brief Close the slot window."""
        self.setVisible(False)
        self.setActive(False)

    def _closeByCancel(self) -> None:
        Manager.playSE(GameSystem.getCancelSE())
        self.close()
        if self._windowEquipSelect is not None:
            self._windowEquipSelect.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback()


class WindowEquipSelect(WindowSelectable):
    r"""\brief Available-equip window with grid display filtered by slot.

    Shows owned equips matching the selected slot with icons, counts, name and description.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        player,
        windowEquipSlot: Optional[WindowEquipSlot] = None,
        onEquip: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the available-equip window.

        - \param rect The window rectangle.
        - \param player The player instance.
        - \param windowEquipSlot The equipped-slot window for focus switching and refresh.
        - \param onEquip Optional callback invoked after equipping an item.
        """
        super().__init__(rect, None, 32, 32)
        self._player = player
        self._windowEquipSlot = windowEquipSlot
        self._onEquipCallback = onEquip
        self._slotKey: str = ""
        self._equipList: List[Optional[str]] = []
        self._equipCounts: Dict[str, int] = {}
        self._lastDescIndex: Optional[int] = None
        self._descNameText = PlainText(UI.DefaultFont, "", 18, Text.Style.Bold)
        self._descText = PlainText(UI.DefaultFont, "", 14)
        self.addChild(self._descNameText)
        self.addChild(self._descText)
        self.setActive(False)
        self.setVisible(False)

    def setEquipSlotWindow(self, windowEquipSlot: WindowEquipSlot) -> None:
        r"""\brief Set the equipped-slot window reference.

        - \param windowEquipSlot The equipped-slot window.
        """
        self._windowEquipSlot = windowEquipSlot

    def _resizeCanvas(self, target: Canvas, width: int, height: int) -> None:
        target._size = Vector2u(width, height)
        target._canvas.resize(Math.ToVector2u(Vector2f(width, height) * Engine.Scale))
        target.setTexture(target._canvas.getTexture(), True)
        target.setView(target.getDefaultView())

    def refreshForSlot(self, slotKey: str) -> None:
        r"""\brief Rebuild the equip list for the given slot.

        - \param slotKey The equipment slot identifier to filter by.
        """
        self._slotKey = slotKey
        descHeight = 64
        contentWidth = self._inRect.size.x - 32
        self._descMaxWidth = contentWidth
        contentHeight = self._inRect.size.y - 32 - descHeight
        self._resizeCanvas(self.content, contentWidth, contentHeight)
        listView = ListView(
            IntRect(Vector2i(0, 0), Vector2i(contentWidth, contentHeight)),
            32,
            True,
            6,
        )
        equipData = Data.getGeneralData("Equip")
        members = equipData.get("members", {})
        playerEquips = self._player._equips if hasattr(self._player, "_equips") else {}
        currentEquipped = self._player.getEquipInfo(slotKey)
        orderedEquips: List[Optional[str]] = []
        if currentEquipped:
            orderedEquips.append(None)
        self._equipCounts = {}
        for equipID in members.keys():
            if equipID in playerEquips and members.get(equipID, {}).get("slot") == slotKey:
                orderedEquips.append(equipID)
                self._equipCounts[equipID] = playerEquips[equipID]
        self._equipList = orderedEquips
        for entry in orderedEquips:
            if entry is None:
                cell = _UnequipCell()
            else:
                member = members.get(entry, {})
                iconPath = member.get("icon", "")
                iconTex = _loadEquipIcon(iconPath)
                cell = _EquipCell(iconTex, self._equipCounts.get(entry, 1))
            cell.addConfirmCallback(lambda obj, kwargs: self._onConfirmAction())
            self._applyItem(cell)
            listView.addChild(cell)
        self.setListView(listView)
        self.index = 0 if len(listView.getChildren()) > 0 else None
        self._lastDescIndex = None
        self._descNameText.setPosition(Vector2f(16, float(contentHeight + 24)))
        self._descText.setPosition(Vector2f(16, float(contentHeight + 48)))
        self._updateDescription()

    def update(self, deltaTime: float) -> None:
        r"""\brief Update equip window and refresh description on index change.

        - \param deltaTime Elapsed time in seconds.
        """
        super().update(deltaTime)
        if self._lastDescIndex != self.index:
            self._lastDescIndex = self.index
            self._updateDescription()

    def _updateDescription(self) -> None:
        if self.index is None or self.index >= len(self._equipList):
            self._descNameText.setString("")
            self._descText.setString("")
            return
        entry = self._equipList[self.index]
        if entry is None:
            self._descNameText.setString(LOC("EQUIP_UNEQUIP"))
            self._descText.setString(_wrapDesc(LOC("EQUIP_UNEQUIP_DESC"), self._descMaxWidth))
            return
        members = Data.getGeneralData("Equip").get("members", {})
        self._descNameText.setString(members.get(entry, {}).get("name", "").format(**LOC_D()))
        raw_desc = members.get(entry, {}).get("desc", "").format(**LOC_D())
        self._descText.setString(_wrapDesc(raw_desc, self._descMaxWidth))

    def _focusSlotWindow(self) -> None:
        if self._windowEquipSlot is None:
            return
        self._closeByCancel()

    def _closeByCancel(self) -> None:
        r"""\brief Close this window and return focus to the slot list."""
        Manager.playSE(GameSystem.getCancelSE())
        self.close()
        if self._windowEquipSlot is not None:
            self._windowEquipSlot.setActive(True)
            self._windowEquipSlot._redrawIfVisible()

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle cancel, confirm, and focus-switch keys.

        - \param kwargs Event data.
        """
        if not self.getActive():
            return
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._closeByCancel()
            return
        if Input.isActionTriggered(Input.getConfirmKeys(), handled=True):
            self._onConfirmAction()
            return
        if Input.isActionTriggered(Input.getLeftKeys(), handled=True):
            columns = self._getColumns()
            if self.index is None or (columns != 1 and self.index % columns == 0):
                self._focusSlotWindow()
                return
        return super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Handle mouse cancel to close this window."""
        if not self.getActive() or not self.getVisible():
            return
        if Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True):
            self._closeByCancel()

    def open(self) -> None:
        r"""\brief Open the available-equip window without taking focus."""
        self.setVisible(False)
        self.setActive(False)

    def close(self) -> None:
        r"""\brief Close the available-equip window."""
        self.setVisible(False)
        self.setActive(False)

    def _onConfirmAction(self) -> None:
        if self.index is None or self.index >= len(self._equipList):
            return
        entry = self._equipList[self.index]
        slotKey = self._slotKey
        currentEquipped = self._player.getEquipInfo(slotKey)
        Manager.playSE(GameSystem.getEquipSE())
        if entry is None or entry == currentEquipped:
            if currentEquipped:
                self._player.unequip(slotKey)
        else:
            self._player.equip(entry)
        if self._windowEquipSlot is not None:
            self._windowEquipSlot._refreshSlots()
        self.refreshForSlot(slotKey)
        if self._onEquipCallback is not None:
            self._onEquipCallback()
