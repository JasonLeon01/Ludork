# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Any, Callable, Dict, Optional, Union, Tuple
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
from .Base import WindowSelectable
from ..System import System as GameSystem
from ..Infos.ItemInfo import ItemInfo
from .. import Data

_UNUSABLE_ICON_ALPHA = 160


class _ItemCell(Canvas, FunctionalBase):
    r"""\brief Single item cell displaying icon and optional count.

    Shows the item icon with reduced opacity for unusable items,
    and a count label at bottom-right for cost items.
    """

    def __init__(self, iconTexture: Optional[Texture], usable: bool, cost: bool, count: int) -> None:
        r"""\brief Construct an item cell.

        - \param iconTexture The loaded item icon texture, or None if no icon available.
        - \param usable Whether the item is usable.
        - \param cost Whether the item has a cost/quantity.
        - \param count The quantity of this item in inventory.
        """
        Canvas.__init__(self, ((0, 0), (32, 32)))
        FunctionalBase.__init__(self)
        if iconTexture is not None:
            icon = FImage(iconTexture)
            if not usable:
                icon.setColor(Color(255, 255, 255, _UNUSABLE_ICON_ALPHA))
            self.addChild(icon)
        if cost:
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


def _loadItemIcon(iconPath: str) -> Optional[Texture]:
    r"""\brief Load an item icon texture from the icon file path.

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


class WindowItem(WindowSelectable):
    r"""\brief Item inventory window with grid display.

    Shows player inventory items in a grid with icons and counts.
    Uses WindowSelectable for keyboard/mouse navigation.
    """

    def __init__(
        self,
        rect: Union[IntRect, Tuple[Pair[int], Pair[int]]],
        player,
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the item window.

        - \param rect The window rectangle.
        - \param player The player instance with inventory.
        - \param onClose Optional callback invoked when the window is closed.
        """
        super().__init__(rect, None, 32, 32)
        self._onCloseCallback = onClose
        self._onUseCallback: Optional[Callable[[], None]] = None
        self._player = player
        self._lastDescIndex: Optional[int] = None
        self._descNameText = PlainText(UI.DefaultFont, "", 18, Text.Style.Bold)
        self._descText = PlainText(UI.DefaultFont, "", 14)
        self.addChild(self._descNameText)
        self.addChild(self._descText)
        self._refreshItems()
        self.setActive(False)
        self.setVisible(False)

    def _resizeCanvas(self, target: Canvas, width: int, height: int) -> None:
        target._size = Vector2u(width, height)
        target._canvas.resize(Math.ToVector2u(Vector2f(width, height) * Engine.Scale))
        target.setTexture(target._canvas.getTexture(), True)
        target.setView(target.getDefaultView())

    def _refreshItems(self) -> None:
        r"""\brief Rebuild the item list from the player's current inventory."""
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
        itemData = Data.getAllGeneralItemData()
        playerItems = self._player._items if hasattr(self._player, "_items") else {}
        orderedItems = []
        for itemID in itemData.keys():
            if itemID in playerItems:
                orderedItems.append((itemID, playerItems[itemID]))
        self._itemList = orderedItems
        for itemID, count in orderedItems:
            member = itemData.get(itemID, {})
            iconPath = member.get("icon", "")
            usable = member.get("usable", True)
            cost = member.get("cost", True)
            iconTex = _loadItemIcon(iconPath)
            cell = _ItemCell(iconTex, usable, cost, count)
            cell.addConfirmCallback(lambda obj, kwargs: self._onUseItem())
            self._applyItem(cell)
            listView.addChild(cell)
        self.setListView(listView)
        self.index = 0 if len(listView.getChildren()) > 0 else None
        self._descNameText.setPosition(Vector2f(16, float(contentHeight + 24)))
        self._descText.setPosition(Vector2f(16, float(contentHeight + 48)))
        self._updateDescription()

    def update(self, deltaTime: float) -> None:
        r"""\brief Update item window and render item cells.

        - \param deltaTime Elapsed time in seconds.
        """
        super().update(deltaTime)
        if self._lastDescIndex != self.index:
            self._lastDescIndex = self.index
            self._updateDescription()

    def _wrapDesc(self, text: str) -> str:
        from Engine import Scale

        charSize = int(14 * Scale)
        maxW = self._descMaxWidth * Scale
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

    def _updateDescription(self) -> None:
        if self.index is None or not hasattr(self, "_itemList") or self.index >= len(self._itemList):
            self._descNameText.setString("")
            self._descText.setString("")
            return
        itemID, _ = self._itemList[self.index]
        self._descNameText.setString(Data.getGeneralItemData(itemID).get("name", "").format(**LOC_D()))
        raw_desc = Data.getGeneralItemData(itemID).get("desc", "").format(**LOC_D())
        self._descText.setString(self._wrapDesc(raw_desc))

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        r"""\brief Handle cancel and confirm keys for the item window.

        - \param kwargs Event data.
        """
        if not self.getActive():
            return
        if Input.isActionTriggered(Input.getCancelKeys(), handled=True):
            self._closeByCancel()
            return
        return super().onKeyDown(kwargs)

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Handle mouse cancel to close the item window."""
        if not self.getActive() or not self.getVisible():
            return
        if Input.isMouseButtonTriggered(Input.Mouse.Button.Right, handled=True):
            self._closeByCancel()

    def open(self) -> None:
        r"""\brief Open the item window, refreshing inventory first."""
        self._refreshItems()
        self.setVisible(True)
        self.setActive(True)

    def close(self) -> None:
        r"""\brief Close the item window."""
        self.setVisible(False)
        self.setActive(False)

    def _onUseItem(self) -> None:
        if self.index is None or not hasattr(self, "_itemList") or self.index >= len(self._itemList):
            return
        itemID, _ = self._itemList[self.index]
        itemInfo = Data.getGeneralItemData(itemID)
        if not itemInfo.get("usable", True):
            return
        Manager.playSE(GameSystem.getDecisionSE())
        info = ItemInfo()
        info.ID = itemID
        info.initInfo(Data)
        info.triggerEvent("onUse")
        self.close()
        if self._onUseCallback is not None:
            self._onUseCallback()

    def _closeByCancel(self) -> None:
        Manager.playSE(GameSystem.getCancelSE())
        self.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback()
