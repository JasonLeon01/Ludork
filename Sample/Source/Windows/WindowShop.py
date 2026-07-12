# -*- encoding: utf-8 -*-

from __future__ import annotations
from typing import Callable, Dict, List, Optional, Tuple, Any
from Engine import (
    Color,
    Input,
    Pair,
    Text,
    Texture,
    UI,
    Vector2f,
)
from Engine.UI import Canvas, ListView
from Engine.UI.Base import FunctionalBase
from Engine.UI.FunctionalUI import FImage, FPlainText
from Global import Manager, System as GlobalSystem
from .Base import WindowSelectable
from .WindowCommand import WindowCommand
from .. import Data
from ..System import System as GameSystem


SHOP_MODE_BUY = "buy"
SHOP_MODE_SELL = "sell"

_SHOP_COMMAND_HEIGHT = 64
_SHOP_ITEM_SIZE = 352
_SHOP_WIDTH = 352
_SHOP_ITEM_ROW_HEIGHT = 32
_SHOP_ITEM_COLUMNS = 2
_SHOP_DISABLED_ALPHA = 120
_SHOP_DISABLED_TEXT_COLOUR = Color(160, 160, 160, 255)
_SHOP_TEXT_SIZE = 12
_SHOP_TEXT_RIGHT_PAD = 2
_SHOP_TEXT_Y_OFFSET = 17

_DEFAULT_COMMAND_RECT: Tuple[Pair[int], Pair[int]] = ((144, 32), (_SHOP_WIDTH, _SHOP_COMMAND_HEIGHT))
_DEFAULT_ITEM_RECT: Tuple[Pair[int], Pair[int]] = ((144, 96), (_SHOP_ITEM_SIZE, _SHOP_ITEM_SIZE))


class _ShopCell(Canvas, FunctionalBase):
    r"""\brief Shop item cell displaying an icon and right-aligned numeric text."""

    def __init__(self, width: int, iconTexture: Optional[Texture], textValue: str, available: bool) -> None:
        r"""\brief Construct a shop item cell.

        - \param width Cell width in logical UI units.
        - \param iconTexture Item icon texture, or None.
        - \param textValue Right-aligned price/count text.
        - \param available Whether the item can currently be confirmed.
        """
        Canvas.__init__(self, ((0, 0), (width, _SHOP_ITEM_ROW_HEIGHT)))
        FunctionalBase.__init__(self)
        self.available = available
        if iconTexture is not None:
            icon = FImage(iconTexture)
            if not available:
                icon.setColour(Color(255, 255, 255, _SHOP_DISABLED_ALPHA))
            self.addChild(icon)
        text = FPlainText(UI.DefaultFont, textValue, _SHOP_TEXT_SIZE)
        text.setLineAlignment(Text.LineAlignment.Right)
        if not available:
            text.setColour(_SHOP_DISABLED_TEXT_COLOUR)
        text.setPosition(
            Vector2f(
                width - _SHOP_TEXT_RIGHT_PAD,
                _SHOP_TEXT_Y_OFFSET,
            )
        )
        self.addChild(text)

    def getChildren(self):
        return []

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the cell and render to its internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        self._buildRenderQueue()
        self.render()


def _loadItemIcon(iconPath: str) -> Optional[Texture]:
    if not iconPath:
        return None
    try:
        if "/" in iconPath or "\\" in iconPath:
            parts = iconPath.replace("\\", "/").split("/")
            if len(parts) >= 2:
                return Manager.loadTexture("/".join(parts[:-1]), parts[-1])
        return Manager.loadTexture("Characters/items", iconPath)
    except Exception:
        return None


class WindowShopCommand(WindowCommand):
    r"""\brief Horizontal buy/sell command bar for the shop."""

    def __init__(
        self,
        rect: Tuple[Pair[int], Pair[int]],
        owner: "WindowShop",
    ) -> None:
        r"""\brief Construct the shop command bar.

        - \param rect The command window rectangle.
        - \param owner The shop coordinator.
        """
        commands = {
            "Buy": {
                "text": LOC("SHOP_BUY"),
                "callback": lambda obj, kwargs: owner.confirmCommand(),
            },
            "Sell": {
                "text": LOC("SHOP_SELL"),
                "callback": lambda obj, kwargs: owner.confirmCommand(),
            },
        }
        super().__init__(rect, commands, rectHeight=32, columns=2)
        self._owner = owner
        self._lastIndex = self.index

    def onTick(self, deltaTime: float) -> None:
        super().onTick(deltaTime)
        if self.index != self._lastIndex:
            self._lastIndex = self.index
            self._owner.setMode(SHOP_MODE_SELL if self.index == 1 else SHOP_MODE_BUY)

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if Input.isActionTriggered(Input.getCancelKeys(), handled=False):
            self._owner.closeByCancel()
            Input.isActionTriggered(Input.getCancelKeys(), handled=True)
            return
        super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        if kwargs["button"] == Input.Mouse.Button.Right:
            self._owner.closeByCancel()
            return True
        return False


class WindowShopItem(WindowSelectable):
    r"""\brief Two-column shop item list."""

    def __init__(
        self,
        rect: Tuple[Pair[int], Pair[int]],
        owner: "WindowShop",
    ) -> None:
        r"""\brief Construct the shop item list.

        - \param rect The item window rectangle.
        - \param owner The shop coordinator.
        """
        super().__init__(rect, None, None, _SHOP_ITEM_ROW_HEIGHT)
        self._owner = owner
        self._itemIDs: List[str] = []
        self._cellAvailable: List[bool] = []

    def refreshItems(self, itemIDs: List[str], availableMap: Dict[str, bool], valueMap: Dict[str, int]) -> None:
        r"""\brief Rebuild the displayed shop item list.

        - \param itemIDs Ordered item IDs to display.
        - \param availableMap Item availability by ID.
        - \param valueMap Right-side numeric value by ID.
        """
        previousIndex = self.index
        previousItemID = self.getCurrentItemID()
        self._itemIDs = itemIDs
        self._cellAvailable = []
        listView = ListView(self.content.getNoTranslationRect(), _SHOP_ITEM_ROW_HEIGHT, True, _SHOP_ITEM_COLUMNS)
        self.setListView(listView)
        cellWidth = self._getRectWidth()
        itemData = Data.getAllGeneralItemData()
        for itemID in itemIDs:
            member = itemData.get(itemID, {})
            available = availableMap.get(itemID, True)
            self._cellAvailable.append(available)
            cell = _ShopCell(cellWidth, _loadItemIcon(member.get("icon", "")), str(valueMap.get(itemID, 0)), available)
            cell.addConfirmCallback(lambda obj, kwargs: self._owner.confirmItem())
            listView.addChild(cell)
        if len(itemIDs) == 0:
            self.index = None
        elif previousItemID in itemIDs:
            self.index = itemIDs.index(previousItemID)
        elif previousIndex is not None:
            self.index = min(previousIndex, len(itemIDs) - 1)
        else:
            self.index = 0
        if self._rect.getParent() is not None:
            self.content.removeChild(self._rect)

    def getCurrentItemID(self) -> Optional[str]:
        if self.index is None or self.index >= len(self._itemIDs):
            return None
        return self._itemIDs[self.index]

    def isCurrentAvailable(self) -> bool:
        if self.index is None or self.index >= len(self._cellAvailable):
            return False
        return self._cellAvailable[self.index]

    def onKeyDown(self, kwargs: Dict[str, Any]) -> None:
        if Input.isActionTriggered(Input.getCancelKeys(), handled=False):
            self._owner.cancelItemSelection()
            Input.isActionTriggered(Input.getCancelKeys(), handled=True)
            return
        super().onKeyDown(kwargs)

    def onMouseButtonDown(self, kwargs: Dict[str, Any]) -> bool:
        if kwargs["button"] == Input.Mouse.Button.Right:
            self._owner.cancelItemSelection()
            return True
        return False


class WindowShop:
    r"""\brief Integrated shop UI with optional command bar and item list."""

    def __init__(
        self,
        player,
        commandRect: Tuple[Pair[int], Pair[int]] = _DEFAULT_COMMAND_RECT,
        itemRect: Tuple[Pair[int], Pair[int]] = _DEFAULT_ITEM_RECT,
        onClose: Optional[Callable[[], None]] = None,
    ) -> None:
        r"""\brief Construct the shop coordinator.

        - \param player The player whose inventory and GOLD are modified.
        - \param commandRect Rectangle for the buy/sell command bar.
        - \param itemRect Rectangle for the item list window.
        - \param onClose Callback invoked after the shop closes.
        """
        self._player = player
        self._onCloseCallback = onClose
        self._commandWindow = WindowShopCommand(commandRect, self)
        self._itemWindow = WindowShopItem(itemRect, self)
        self._itemTopLeft = Vector2f(float(itemRect[0][0]), float(itemRect[0][1]))
        self._buyItemIDs: List[str] = []
        self._canSell = True
        self._mode = SHOP_MODE_BUY
        self._closed = True
        self.close()

    def getCommandWindow(self) -> WindowShopCommand:
        r"""\brief Get the shop command window.

        - \return The command window.
        """
        return self._commandWindow

    def getItemWindow(self) -> WindowShopItem:
        r"""\brief Get the shop item window.

        - \return The item window.
        """
        return self._itemWindow

    def setPlayer(self, player) -> None:
        r"""\brief Rebind the player instance used by the shop.

        - \param player The new player instance.
        """
        self._player = player

    def getVisible(self) -> bool:
        r"""\brief Return whether the shop UI is visible.

        - \return True when the item window is visible.
        """
        return self._itemWindow.getVisible()

    def isClosed(self) -> bool:
        r"""\brief Return whether the latest shop session has closed.

        - \return True after close.
        """
        return self._closed

    def open(self, buyItemIDs: List[str], canSell: bool) -> None:
        r"""\brief Open the shop with the provided buy list and sell flag.

        - \param buyItemIDs Item IDs available for purchase.
        - \param canSell Whether the sell command is available.
        """
        self._buyItemIDs = self._normalizeBuyItems(buyItemIDs)
        self._canSell = bool(canSell)
        self._mode = SHOP_MODE_BUY
        self._closed = False
        self._commandWindow.index = 0
        self._commandWindow._lastIndex = 0
        self._refreshItems()
        if self._canSell:
            self._commandWindow.setVisible(True)
            self._commandWindow.setActive(True)
            self._itemWindow.setPosition(self._itemTopLeft)
            self._itemWindow.setActive(False)
            self._commandWindow.requestKeyboardFocus()
        else:
            gameSize = GlobalSystem.getGameSize()
            topLeft = Vector2f(float((gameSize.x - _SHOP_ITEM_SIZE) / 2), float((gameSize.y - _SHOP_ITEM_SIZE) / 2))
            self._itemWindow.setPosition(topLeft)
            self._commandWindow.setVisible(False)
            self._commandWindow.setActive(False)
            self._itemWindow.setActive(True)
            self._itemWindow.requestKeyboardFocus()
        self._itemWindow.setVisible(True)

    def close(self) -> None:
        r"""\brief Close and deactivate both shop windows."""
        self._commandWindow.setVisible(False)
        self._commandWindow.setActive(False)
        self._itemWindow.setVisible(False)
        self._itemWindow.setActive(False)
        self._closed = True

    def closeByCancel(self) -> None:
        r"""\brief Close the whole shop via cancel input."""
        Manager.playSE(GameSystem.getCancelSE())
        self._closeAndNotify()

    def setMode(self, mode: str) -> None:
        r"""\brief Switch buy/sell mode and refresh the item list.

        - \param mode The shop mode.
        """
        if mode not in (SHOP_MODE_BUY, SHOP_MODE_SELL):
            return
        if not self._canSell and mode == SHOP_MODE_SELL:
            return
        self._mode = mode
        self._refreshItems()

    def confirmCommand(self) -> None:
        r"""\brief Confirm buy/sell mode and move focus to the item list."""
        self.setMode(SHOP_MODE_SELL if self._commandWindow.index == 1 else SHOP_MODE_BUY)
        Manager.playSE(GameSystem.getDecisionSE())
        self._commandWindow.setActive(False)
        self._itemWindow.setActive(True)
        self._itemWindow.requestKeyboardFocus()

    def cancelItemSelection(self) -> None:
        r"""\brief Cancel item selection, returning to command bar or closing shop."""
        Manager.playSE(GameSystem.getCancelSE())
        if not self._canSell:
            self._closeAndNotify()
            return
        self._itemWindow.setActive(False)
        self._commandWindow.setActive(True)
        self._commandWindow.requestKeyboardFocus()

    def confirmItem(self) -> None:
        r"""\brief Confirm the selected item and perform buy/sell."""
        itemID = self._itemWindow.getCurrentItemID()
        if itemID is None:
            Manager.playSE(GameSystem.getBuzzerSE())
            return
        if self._mode == SHOP_MODE_BUY:
            self._buyItem(itemID)
        else:
            self._sellItem(itemID)

    def _normalizeBuyItems(self, buyItemIDs: List[str]) -> List[str]:
        itemData = Data.getAllGeneralItemData()
        result: List[str] = []
        for itemID in buyItemIDs:
            itemKey = str(itemID)
            if itemKey in itemData and itemKey not in result:
                result.append(itemKey)
        return result

    def _refreshItems(self) -> None:
        if self._mode == SHOP_MODE_BUY:
            itemIDs = self._buyItemIDs
            availableMap = {itemID: self._canBuy(itemID) for itemID in itemIDs}
            valueMap = {itemID: self._getItemPrice(itemID) for itemID in itemIDs}
        else:
            itemIDs = self._getSellableItems()
            availableMap = {itemID: True for itemID in itemIDs}
            valueMap = {itemID: self._player.getItemCount(itemID) for itemID in itemIDs}
        self._itemWindow.refreshItems(itemIDs, availableMap, valueMap)

    def _getSellableItems(self) -> List[str]:
        itemData = Data.getAllGeneralItemData()
        playerItems = self._player._items if hasattr(self._player, "_items") else {}
        result: List[str] = []
        for itemID in itemData.keys():
            if itemID in playerItems and playerItems[itemID] > 0 and self._getItemPrice(itemID) > 0:
                result.append(itemID)
        return result

    def _getItemPrice(self, itemID: str) -> int:
        itemInfo = Data.getGeneralItemData(itemID)
        return int(itemInfo.get("price", 0))

    def _canBuy(self, itemID: str) -> bool:
        return self._player.infoComp.GOLD >= self._getItemPrice(itemID)

    def _buyItem(self, itemID: str) -> None:
        price = self._getItemPrice(itemID)
        if not self._itemWindow.isCurrentAvailable() or self._player.infoComp.GOLD < price:
            Manager.playSE(GameSystem.getBuzzerSE())
            self._refreshItems()
            return
        self._player.infoComp.GOLD -= price
        self._player.addItem(itemID, 1)
        Manager.playSE(GameSystem.getShopSE())
        self._refreshItems()

    def _sellItem(self, itemID: str) -> None:
        price = self._getItemPrice(itemID)
        if price <= 0 or not self._player.removeItem(itemID, 1):
            Manager.playSE(GameSystem.getBuzzerSE())
            self._refreshItems()
            return
        self._player.infoComp.GOLD += price
        Manager.playSE(GameSystem.getShopSE())
        self._refreshItems()

    def _closeAndNotify(self) -> None:
        self.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback()
