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
    Color,
    Vector2f,
    Input,
    UI,
    Text,
)
from Engine.UI import Canvas, ListView, PlainText
from Engine.UI.Base import FunctionalBase
from Engine.UI.FunctionalUI import FImage, FPlainText
from Engine.Utils import Math
from Global import Manager
from .Base import WindowBase, WindowSelectable
from ..System import System as GameSystem
from .. import Data

_SLOT_ROW_HEIGHT = 32
_SLOT_FONT_SIZE = 14
_SLOT_TEXT_PAD = 32
SLOT_Y_OFFSET = 8
_EQUIP_CELL_SIZE = 32
_EQUIP_STATUS_FONT_SIZE = 16
_EQUIP_STATUS_ROW_HEIGHT = 20
_EQUIP_STATUS_MAX_ROWS = 3
_EQUIP_STATUS_SLOT_DESC_NAME_Y = 0
_EQUIP_STATUS_SLOT_DESC_TEXT_Y = 24
_EQUIP_STATUS_DESC_NAME_Y = 76
_EQUIP_STATUS_DESC_TEXT_Y = 100
_EQUIP_STATUS_DESC_NAME_SIZE = 18
_EQUIP_STATUS_DESC_SIZE = 14


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

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the cell and render to internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        self._buildRenderQueue()
        self.render()


class _UnequipCell(Canvas, FunctionalBase):
    r"""\brief Empty unequip action cell."""

    def __init__(self) -> None:
        Canvas.__init__(self, ((0, 0), (32, 32)))
        FunctionalBase.__init__(self)

    def getChildren(self):
        return []

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the cell and render to internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        self._buildRenderQueue()
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

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the cell and render to internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        self._buildRenderQueue()
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


class WindowEquipStatus(WindowBase):
    r"""\brief Equipment detail window with stat delta preview and description."""

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        player,
    ) -> None:
        r"""\brief Construct the equipment status window.

        - \param rect The window rectangle.
        - \param player The player instance.
        """
        super().__init__(rect)
        self._player = player
        self._slotKey: str = ""
        self._changeTexts: List[PlainText] = []
        self._descNameText = PlainText(UI.DefaultFont, "", _EQUIP_STATUS_DESC_NAME_SIZE, Text.Style.Bold)
        self._descText = PlainText(UI.DefaultFont, "", _EQUIP_STATUS_DESC_SIZE)
        self._descNameText.setPosition(Vector2f(0.0, float(_EQUIP_STATUS_DESC_NAME_Y)))
        self._descText.setPosition(Vector2f(0.0, float(_EQUIP_STATUS_DESC_TEXT_Y)))
        self.content.addChild(self._descNameText)
        self.content.addChild(self._descText)
        self.setActive(False)
        self.setVisible(False)

    def setPlayer(self, player) -> None:
        r"""\brief Rebind the player instance used for equipment comparisons.

        - \param player The player instance.
        """
        self._player = player

    def openForSlot(self, slotKey: str) -> None:
        r"""\brief Open the detail window for the current equipment slot.

        - \param slotKey Equipment slot identifier.
        """
        self.refreshForSlot(slotKey)
        self.setVisible(True)
        self.setActive(False)

    def close(self) -> None:
        r"""\brief Close the detail window."""
        self.setVisible(False)
        self.setActive(False)

    def refreshForEquip(self, slotKey: str, candidateEquipID: Optional[str], showUnequip: bool = False) -> None:
        r"""\brief Refresh stat changes and description for a selected equipment candidate.

        - \param slotKey Equipment slot identifier.
        - \param candidateEquipID Candidate equipment ID, or None for no candidate.
        - \param showUnequip Whether the candidate is the unequip command.
        """
        self._slotKey = slotKey
        currentEquipID = self._player.getEquipInfo(slotKey)
        currentAttrs = self._getAttrPlus(currentEquipID)
        candidateAttrs = {} if showUnequip else self._getAttrPlus(candidateEquipID)
        self._refreshChangeRows(currentAttrs, candidateAttrs)
        self._setDescriptionPosition(_EQUIP_STATUS_DESC_NAME_Y, _EQUIP_STATUS_DESC_TEXT_Y)
        self._refreshDescription(candidateEquipID, showUnequip)

    def refreshForSlot(self, slotKey: str) -> None:
        r"""\brief Refresh description for the current equipped item in a slot.

        - \param slotKey Equipment slot identifier.
        """
        self._slotKey = slotKey
        self._clearChangeTexts()
        self._setDescriptionPosition(_EQUIP_STATUS_SLOT_DESC_NAME_Y, _EQUIP_STATUS_SLOT_DESC_TEXT_Y)
        currentEquipID = self._player.getEquipInfo(slotKey)
        self._refreshDescription(currentEquipID or None, False)

    def _refreshChangeRows(self, currentAttrs: Dict[str, int], candidateAttrs: Dict[str, int]) -> None:
        self._clearChangeTexts()
        rowIndex = 0
        for attrKey in self._getAttrKeys(candidateAttrs, currentAttrs):
            delta = candidateAttrs.get(attrKey, 0) - currentAttrs.get(attrKey, 0)
            if delta == 0:
                continue
            self._addChangeRow(attrKey, delta, rowIndex)
            rowIndex += 1
            if rowIndex >= _EQUIP_STATUS_MAX_ROWS:
                break

    def _addChangeRow(self, attrKey: str, delta: int, rowIndex: int) -> None:
        y = float(rowIndex * _EQUIP_STATUS_ROW_HEIGHT)
        labelText = PlainText(UI.DefaultFont, LOC(attrKey), _EQUIP_STATUS_FONT_SIZE)
        labelText.setPosition(Vector2f(0.0, y))
        deltaText = PlainText(
            UI.DefaultFont,
            f"+{delta}" if delta > 0 else str(delta),
            _EQUIP_STATUS_FONT_SIZE,
            fillColor=Color.Green if delta > 0 else Color.Red,
        )
        self._setRightAligned(deltaText, y)
        self.content.addChild(labelText)
        self.content.addChild(deltaText)
        self._changeTexts.extend([labelText, deltaText])

    def _refreshDescription(self, candidateEquipID: Optional[str], showUnequip: bool) -> None:
        descMaxWidth = max(1, int(self.content.getSize().x))
        if showUnequip:
            self._descNameText.setString(LOC("EQUIP_UNEQUIP"))
            self._descText.setString(_wrapDesc(LOC("EQUIP_UNEQUIP_DESC"), descMaxWidth))
            return
        if candidateEquipID is None:
            self._descNameText.setString("")
            self._descText.setString("")
            return
        equipInfo = Data.getGeneralEquipData(candidateEquipID)
        self._descNameText.setString(equipInfo.get("name", "").format(**LOC_D()))
        rawDesc = equipInfo.get("desc", "").format(**LOC_D())
        self._descText.setString(_wrapDesc(rawDesc, descMaxWidth))

    def _clearChangeTexts(self) -> None:
        for text in self._changeTexts:
            if text.getParent() is self.content:
                self.content.removeChild(text)
        self._changeTexts = []

    def _setDescriptionPosition(self, nameY: int, descY: int) -> None:
        self._descNameText.setPosition(Vector2f(0.0, float(nameY)))
        self._descText.setPosition(Vector2f(0.0, float(descY)))

    def _getAttrPlus(self, equipID: Optional[str]) -> Dict[str, int]:
        if not equipID:
            return {}
        attrPlus = Data.getGeneralEquipData(equipID).get("attrPlus", {})
        if not isinstance(attrPlus, dict):
            return {}
        result: Dict[str, int] = {}
        for attrKey, attrValue in attrPlus.items():
            try:
                result[str(attrKey)] = int(attrValue)
            except (TypeError, ValueError):
                continue
        return result

    def _getAttrKeys(self, firstAttrs: Dict[str, int], secondAttrs: Dict[str, int]) -> List[str]:
        result: List[str] = []
        for attrs in [firstAttrs, secondAttrs]:
            for attrKey in attrs.keys():
                if attrKey not in result:
                    result.append(attrKey)
        return result

    def _setRightAligned(self, text: PlainText, y: float) -> None:
        bounds = text.getLocalBounds()
        contentWidth = float(self.content.getSize().x)
        text.setPosition(Vector2f(contentWidth - bounds.size.x - bounds.position.x, y))


class WindowEquipSlot(WindowSelectable):
    r"""\brief Equipped-slot list window ordered by class slot keys.

    Shows currently equipped item names per slot, or unequipped placeholder text.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        player,
        windowEquipSelect: Optional["WindowEquipSelect"] = None,
        windowEquipStatus: Optional[WindowEquipStatus] = None,
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the equipped-slot window.

        - \param rect The window rectangle.
        - \param player The player instance.
        - \param windowEquipSelect The available-equip window to refresh on slot change.
        - \param windowEquipStatus The detail window to refresh on slot change.
        - \param onClose Optional callback invoked when the window is closed.
        """
        super().__init__(rect, None, None, _SLOT_ROW_HEIGHT)
        self._onCloseCallback = onClose
        self._player = player
        self._windowEquipSelect = windowEquipSelect
        self._windowEquipStatus = windowEquipStatus
        self._slotKeys: List[str] = []
        self._lastSlotIndex: Optional[int] = None
        self._refreshSlots()
        self.setActive(False)
        self.setVisible(False)

    def setEquipSelectWindow(self, windowEquipSelect: "WindowEquipSelect") -> None:
        r"""\brief Set the available-equip window reference.

        - \param windowEquipSelect The available-equip window.
        """
        self._windowEquipSelect = windowEquipSelect

    def setEquipStatusWindow(self, windowEquipStatus: WindowEquipStatus) -> None:
        r"""\brief Set the equipment detail window reference.

        - \param windowEquipStatus The equipment detail window.
        """
        self._windowEquipStatus = windowEquipStatus

    def _getSlotCellData(self, slotKey: str) -> Tuple[Optional[Texture], str]:
        equipID = self._player.getEquipInfo(slotKey)
        if not equipID:
            return None, LOC("EQUIP_UNEQUIPPED")
        equipInfo = Data.getGeneralEquipData(equipID)
        name = equipInfo.get("name", "")
        label = name.format(**LOC_D()) if name else equipID
        iconTex = _loadEquipIcon(equipInfo.get("icon", ""))
        return iconTex, label

    def _refreshSlots(self) -> None:
        r"""\brief Rebuild the slot list from the player's class slot order."""
        savedSlotKey = self._getCurrentSlotKey()
        classSlots = Data.getGeneralClassData(self._player.infoComp.CLASS).get("slot", {})
        self._slotKeys = list(classSlots.keys())
        cellWidth = self._getRectWidth()
        listView = ListView(self.content.getNoTranslationRect(), _SLOT_ROW_HEIGHT, True, 1)
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
        if self._rect.getParent() is not None:
            self.content.removeChild(self._rect)
        self._redrawIfVisible()

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
        if slotKey is None:
            return
        if self._windowEquipStatus is not None and self._windowEquipStatus.getVisible():
            self._windowEquipStatus.refreshForSlot(slotKey)
        if self._windowEquipSelect is not None and self._windowEquipSelect.getVisible():
            self._windowEquipSelect.refreshForSlot(slotKey)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update slot window and notify slot change on index change.

        - \param deltaTime Elapsed time in seconds.
        """
        super().onTick(deltaTime)
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
        if self._windowEquipStatus is not None and slotKey is not None:
            self._windowEquipStatus.setVisible(True)
            self._windowEquipStatus.refreshForSlot(slotKey)
        self.setActive(False)
        self._windowEquipSelect.setVisible(True)
        self._windowEquipSelect.setActive(True)
        self._windowEquipSelect.updateStatus()
        self._windowEquipSelect.requestKeyboardFocus()

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle cancel, confirm, and focus-switch keys.

        - \param kwargs Event data.
        """
        if Input.isActionTriggered(Input.getCancelKeys(), handled=False):
            self._closeByCancel()
            Input.isActionTriggered(Input.getCancelKeys(), handled=True)
            return
        return super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        r"""\brief Handle mouse cancel to close the slot window."""
        if kwargs["button"] == Input.Mouse.Button.Right:
            self._closeByCancel()
            return True
        return False

    def open(self) -> None:
        r"""\brief Open the slot window, refreshing slot list first."""
        self._refreshSlots()
        self.setVisible(True)
        self.setActive(True)
        if self._windowEquipStatus is not None:
            slotKey = self._getCurrentSlotKey()
            if slotKey is not None:
                self._windowEquipStatus.openForSlot(slotKey)
            else:
                self._windowEquipStatus.close()
        if self._windowEquipSelect is not None:
            slotKey = self._getCurrentSlotKey()
            if slotKey is not None:
                self._windowEquipSelect.refreshForSlot(slotKey)
                self._windowEquipSelect.setVisible(True)
                self._windowEquipSelect.setActive(False)
            else:
                self._windowEquipSelect.close()

    def close(self) -> None:
        r"""\brief Close the slot window."""
        self.setVisible(False)
        self.setActive(False)
        if self._windowEquipStatus is not None:
            self._windowEquipStatus.close()

    def _closeByCancel(self) -> None:
        Manager.playSE(GameSystem.getCancelSE())
        self.close()
        if self._windowEquipSelect is not None:
            self._windowEquipSelect.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback()


class WindowEquipSelect(WindowSelectable):
    r"""\brief Available-equip window with grid display filtered by slot.

    Shows owned equips matching the selected slot with icons and counts.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        player,
        windowEquipSlot: Optional[WindowEquipSlot] = None,
        windowEquipStatus: Optional[WindowEquipStatus] = None,
        onEquip: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the available-equip window.

        - \param rect The window rectangle.
        - \param player The player instance.
        - \param windowEquipSlot The equipped-slot window for focus switching and refresh.
        - \param windowEquipStatus The detail window for stat changes and description.
        - \param onEquip Optional callback invoked after equipping an item.
        """
        super().__init__(rect, None, 32, 32)
        self._player = player
        self._windowEquipSlot = windowEquipSlot
        self._windowEquipStatus = windowEquipStatus
        self._onEquipCallback = onEquip
        self._slotKey: str = ""
        self._equipList: List[Optional[str]] = []
        self._equipCounts: Dict[str, int] = {}
        self._lastStatusIndex: Optional[int] = None
        self.setActive(False)
        self.setVisible(False)

    def setEquipSlotWindow(self, windowEquipSlot: WindowEquipSlot) -> None:
        r"""\brief Set the equipped-slot window reference.

        - \param windowEquipSlot The equipped-slot window.
        """
        self._windowEquipSlot = windowEquipSlot

    def setEquipStatusWindow(self, windowEquipStatus: WindowEquipStatus) -> None:
        r"""\brief Set the equipment detail window reference.

        - \param windowEquipStatus The equipment detail window.
        """
        self._windowEquipStatus = windowEquipStatus

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
        contentWidth = self._inRect.size.x - 32
        contentHeight = self._inRect.size.y - 32
        self._resizeCanvas(self.content, contentWidth, contentHeight)
        listView = ListView(
            IntRect(Vector2i(0, 0), Vector2i(contentWidth, contentHeight)),
            _EQUIP_CELL_SIZE,
            True,
            self._getGridColumns(contentWidth),
        )
        equipData = Data.getAllGeneralEquipData()
        playerEquips = self._player._equips if hasattr(self._player, "_equips") else {}
        currentEquipped = self._player.getEquipInfo(slotKey)
        orderedEquips: List[Optional[str]] = []
        if currentEquipped:
            orderedEquips.append(None)
        self._equipCounts = {}
        for equipID in equipData.keys():
            if equipID in playerEquips and equipData.get(equipID, {}).get("slot") == slotKey:
                orderedEquips.append(equipID)
                self._equipCounts[equipID] = playerEquips[equipID]
        self._equipList = orderedEquips
        for entry in orderedEquips:
            if entry is None:
                cell = _UnequipCell()
            else:
                member = equipData.get(entry, {})
                iconPath = member.get("icon", "")
                iconTex = _loadEquipIcon(iconPath)
                cell = _EquipCell(iconTex, self._equipCounts.get(entry, 1))
            cell.addConfirmCallback(lambda obj, kwargs: self._onConfirmAction())
            self._applyItem(cell)
            listView.addChild(cell)
        self.setListView(listView)
        self.index = 0 if len(listView.getChildren()) > 0 else None
        self._lastStatusIndex = None
        if self.getActive():
            self.updateStatus()

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update equip window and refresh description on index change.

        - \param deltaTime Elapsed time in seconds.
        """
        super().onTick(deltaTime)
        if self._lastStatusIndex != self.index:
            self._lastStatusIndex = self.index
            if self.getActive():
                self.updateStatus()

    def updateStatus(self) -> None:
        r"""\brief Refresh the detail window from the current selected equipment."""
        if self._windowEquipStatus is None:
            return
        if self.index is None or self.index >= len(self._equipList):
            self._windowEquipStatus.refreshForEquip(self._slotKey, None)
            return
        entry = self._equipList[self.index]
        self._windowEquipStatus.refreshForEquip(self._slotKey, entry, entry is None)

    def _getGridColumns(self, contentWidth: int) -> int:
        return max(1, int((contentWidth - _EQUIP_CELL_SIZE) / _EQUIP_CELL_SIZE))

    def returnToSlotWindow(self, playSE: bool = True) -> None:
        r"""\brief Return focus to the slot list while keeping this window visible.

        - \param playSE Whether to play the cancel sound effect.
        """
        if playSE:
            Manager.playSE(GameSystem.getCancelSE())
        self.setActive(False)
        self.setVisible(True)
        if self._windowEquipSlot is not None:
            self._windowEquipSlot.setActive(True)
            self._windowEquipSlot.requestKeyboardFocus()
        if self._windowEquipStatus is not None and self._slotKey:
            self._windowEquipStatus.refreshForSlot(self._slotKey)

    def _closeByCancel(self) -> None:
        r"""\brief Handle cancel by returning focus to the slot list."""
        self.returnToSlotWindow()

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle cancel, confirm, and focus-switch keys.

        - \param kwargs Event data.
        """
        if Input.isActionTriggered(Input.getCancelKeys(), handled=False):
            self._closeByCancel()
            Input.isActionTriggered(Input.getCancelKeys(), handled=True)
            return
        return super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        r"""\brief Handle mouse cancel to close this window."""
        if kwargs["button"] == Input.Mouse.Button.Right:
            self._closeByCancel()
            return True
        return False

    def open(self) -> None:
        r"""\brief Open the available-equip window without taking focus."""
        self.setVisible(True)
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
