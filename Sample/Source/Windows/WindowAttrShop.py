# -*- encoding: utf-8 -*-

from __future__ import annotations
import copy
from typing import Any, Callable, Dict, List, Optional, Tuple, Union
from Engine import (
    Color,
    Image as EngineImage,
    Input,
    IntRect,
    Pair,
    Text,
    Texture,
    UI,
    Vector2f,
    Vector2i,
    Vector2u,
)
from Engine.Gameplay.Actors import Actor
from Engine.UI import Canvas, ListView
from Engine.UI.Base import FunctionalBase
from Engine.UI.FunctionalUI import FImage, FPlainText
from Engine.Utils import Inner
from Global import Manager, System as GlobalSystem
from .Base import WindowSelectable
from ..System import System as GameSystem


_ITEM_ROW_HEIGHT = 32
_ITEM_LIST_Y = 144
_AVATAR_SIZE = 32
_DISABLED_COLOUR = Color(160, 160, 160, 255)


class _AttrShopCell(Canvas, FunctionalBase):
    r"""\brief Attribute shop row displaying its purchase summary."""

    def __init__(self, width: int, textValue: str, available: bool) -> None:
        r"""\brief Construct an attribute shop row.

        - \param width Cell width in logical UI units.
        - \param textValue Localised purchase summary.
        - \param available Whether the ability can currently be purchased.
        """
        Canvas.__init__(self, ((0, 0), (width, _ITEM_ROW_HEIGHT)))
        FunctionalBase.__init__(self)
        text = FPlainText(UI.DefaultFont, textValue, UI.DefaultFontSize)
        text.setLineAlignment(Text.LineAlignment.Center)
        text.setPosition(Vector2f(float(width) / 2.0, 8.0))
        if not available:
            text.setColour(_DISABLED_COLOUR)
        self.addChild(text)

    def getChildren(self) -> List[Any]:
        return []

    def onTick(self, deltaTime: float) -> None:
        r"""\brief Update the row and render it to its internal texture.

        - \param deltaTime Elapsed time in seconds.
        """
        self._buildRenderQueue()
        self.render()


class _WindowAttrShopSelectable(WindowSelectable):
    r"""\brief Selectable attribute list hosted inside the shop window."""

    def __init__(
        self,
        rect: Tuple[Pair[int], Pair[int]],
        owner: "WindowAttrShop",
    ) -> None:
        r"""\brief Construct the attribute shop selection window.

        - \param rect Window rectangle.
        - \param owner Attribute shop coordinator.
        """
        super().__init__(rect, None, None, _ITEM_ROW_HEIGHT)
        self._owner = owner
        self._abilityKeys: List[str] = []
        self._cellAvailable: List[bool] = []

    def refresh(
        self,
        abilities: Dict[str, int],
        prices: List[int],
        moneyName: str,
        moneyAmount: int,
    ) -> None:
        r"""\brief Rebuild the ability rows and leave command.

        - \param abilities Mapping of player attribute names to purchased increments.
        - \param prices Purchase prices ordered to match abilities.
        - \param moneyName Player info component attribute used as currency.
        - \param moneyAmount Current amount of the selected currency.
        """
        previousIndex = self.index
        self._abilityKeys = list(abilities.keys())
        self._cellAvailable = []
        contentSize = self.content.getSize()
        listView = ListView(
            IntRect(
                Vector2i(0, _ITEM_LIST_Y),
                Vector2i(int(contentSize.x), max(0, int(contentSize.y) - _ITEM_LIST_Y)),
            ),
            _ITEM_ROW_HEIGHT,
            True,
            1,
        )
        self.setListView(listView)
        cellWidth = self._getRectWidth()
        moneyDisplayName = self._owner.getAttributeDisplayName(moneyName)
        for index, abilityKey in enumerate(self._abilityKeys):
            price = prices[index]
            delta = abilities[abilityKey]
            available = (
                moneyAmount >= price
                and hasattr(self._owner.getPlayer().infoComp, moneyName)
                and hasattr(self._owner.getPlayer().infoComp, abilityKey)
            )
            self._cellAvailable.append(available)
            cell = _AttrShopCell(
                cellWidth,
                self._owner._formatPurchaseText(
                    abilityKey,
                    delta,
                    price,
                    moneyDisplayName,
                ),
                available,
            )
            cell.addConfirmCallback(lambda obj, kwargs: self._owner.confirmItem())
            listView.addChild(cell)
        leaveCell = _AttrShopCell(cellWidth, LOC("SHOP_ATTR_LEAVE"), True)
        leaveCell.addConfirmCallback(lambda obj, kwargs: self._owner.confirmItem())
        listView.addChild(leaveCell)
        self._cellAvailable.append(True)
        if previousIndex is None:
            self.index = 0
        else:
            self.index = min(previousIndex, len(self._abilityKeys))
        if self._rect.getParent() is not None:
            self.content.removeChild(self._rect)

    def getSelectedAbilityKey(self) -> Optional[str]:
        r"""\brief Get the selected player attribute name.

        - \return Attribute name, or None when Leave is selected.
        """
        if self.index is None or self.index < 0 or self.index >= len(self._abilityKeys):
            return None
        return self._abilityKeys[self.index]

    def isCurrentAvailable(self) -> bool:
        r"""\brief Return whether the selected row can be confirmed."""
        if self.index is None or self.index < 0 or self.index >= len(self._cellAvailable):
            return False
        return self._cellAvailable[self.index]

    def onTick(self, deltaTime: float) -> None:
        self._owner._animateAvatar(deltaTime)
        super().onTick(deltaTime)

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

    def _getRectPositionForIndex(self, index: int) -> Vector2f:
        return super()._getRectPositionForIndex(index) + Vector2f(0.0, float(_ITEM_LIST_Y))


class WindowAttrShop:
    r"""\brief Attribute upgrade shop coordinator."""

    _SIZE = 352

    def __init__(self, player: Any, onClose: Optional[Callable[[], None]] = None) -> None:
        r"""\brief Construct the attribute shop.

        - \param player Player whose currency and attributes are modified.
        - \param onClose Callback invoked after the shop closes.
        """
        self._player = player
        self._onCloseCallback = onClose
        self._abilities: Dict[str, int] = {}
        self._priceRef: Optional[Any] = None
        self._fallbackPrice: Any = 0
        self._priceIncrement = 1
        self._moneyName = "GOLD"
        self._closed = True
        self._selectable = _WindowAttrShopSelectable(self._getDefaultRect(), self)
        placeholder = Texture(EngineImage(Vector2u(1, 1), Color.Transparent))
        self._avatarImage = FImage(placeholder)
        self._avatarImage.setPosition(Vector2f(16.0, 16.0))
        self._nameText = FPlainText(UI.DefaultFont, "", 16)
        self._nameText.setPosition(Vector2f(56.0, 24.0))
        self._descText = FPlainText(UI.DefaultFont, "", 14)
        self._descText.setPosition(Vector2f(16.0, 64.0))
        self._priceText = FPlainText(UI.DefaultFont, "", 16)
        self._priceText.setPosition(Vector2f(16.0, 128.0))
        self._selectable.addChild(self._avatarImage)
        self._selectable.addChild(self._nameText)
        self._selectable.addChild(self._descText)
        self._selectable.addChild(self._priceText)
        self._avatarTexture: Optional[Texture] = None
        self._avatarRect: Optional[IntRect] = None
        self._avatarAnimatable = False
        self._avatarSwitchInterval = 0.2
        self._avatarSwitchTimer = 0.0
        self.close()

    def getSelectable(self) -> _WindowAttrShopSelectable:
        r"""\brief Get the shop selection window for UI manager registration."""
        return self._selectable

    def getPlayer(self) -> Any:
        r"""\brief Get the player currently bound to the shop."""
        return self._player

    def setPlayer(self, player: Any) -> None:
        r"""\brief Rebind the player used by the shop.

        - \param player New player instance.
        """
        self._player = player

    def getAttributeDisplayName(self, attributeName: str) -> str:
        r"""\brief Resolve a display name for a player info component attribute.

        - \param attributeName Player info component attribute name.
        - \return Localised display name.
        """
        displayName = LOC(attributeName)
        return displayName.rstrip().rstrip(":：")

    def open(
        self,
        shopActor: Optional[Actor],
        shopName: str,
        shopDescription: str,
        abilities: Dict[str, int],
        priceRef: Union[int, List[int]],
        priceIncrement: int,
        moneyName: str = "GOLD",
        rect: Optional[Tuple[Pair[int], Pair[int]]] = None,
    ) -> None:
        r"""\brief Open the shop with the supplied actor, text, abilities, and price.

        - \param shopActor Actor whose first texture frame is used as the avatar.
        - \param shopName Locale key for the shop name.
        - \param shopDescription Locale key for the shop description.
        - \param abilities Mapping of player attribute names to purchased increments.
        - \param priceRef Mutable reference containing the current shared price.
        - \param priceIncrement Amount added to the shared price after each purchase.
        - \param moneyName Player info component attribute used as currency.
        - \param rect Optional centred shop rectangle.
        """
        self._abilities = {str(key): int(value) for key, value in abilities.items()}
        self._priceRef = priceRef
        self._fallbackPrice = 0 if priceRef is None else self._fallbackPrice
        self._priceIncrement = int(priceIncrement)
        self._moneyName = str(moneyName)
        self._getPrices()
        if rect is not None:
            self._selectable.setPosition(Vector2f(float(rect[0][0]), float(rect[0][1])))
        self._refreshAvatar(shopActor)
        self._nameText.setString(Inner.ApplyStringLocaleFormat(shopName) if shopName else "")
        description = Inner.ApplyStringLocaleFormat(shopDescription) if shopDescription else ""
        self._descText.setString(description.replace("\\n", "\n"))
        self.refreshPriceText()
        self.refreshItems()
        self._closed = False
        self._selectable.setVisible(True)
        self._selectable.setActive(True)
        self._selectable.requestKeyboardFocus()

    def refreshPriceText(self) -> None:
        r"""\brief Refresh the shared price label for scalar prices."""
        priceValue = self._getPriceValue()
        if isinstance(priceValue, (list, tuple)):
            self._priceText.setString("")
            return
        text = Inner.ApplyStringMappingFormat(
            LOC("SHOP_ATTR_PRICE"),
            {"gold": int(priceValue)},
        )
        self._priceText.setString(text)

    def refreshItems(self) -> None:
        r"""\brief Refresh ability availability and displayed prices."""
        self._selectable.refresh(
            self._abilities,
            self._getPrices(),
            self._moneyName,
            int(getattr(self._player.infoComp, self._moneyName, 0)),
        )

    def close(self) -> None:
        r"""\brief Close and deactivate the attribute shop."""
        self._selectable.setVisible(False)
        self._selectable.setActive(False)
        self._closed = True

    def closeByCancel(self) -> None:
        r"""\brief Close the shop via cancel input and notify its owner."""
        if self._closed:
            return
        Manager.playSE(GameSystem.getCancelSE())
        self._closeAndNotify()

    def confirmItem(self) -> None:
        r"""\brief Confirm the selected attribute purchase or Leave command."""
        abilityKey = self._selectable.getSelectedAbilityKey()
        if abilityKey is None:
            self.closeByCancel()
            return
        abilityIndex = list(self._abilities.keys()).index(abilityKey)
        price = self._getPrices()[abilityIndex]
        infoComp = self._player.infoComp
        if (
            not self._selectable.isCurrentAvailable()
            or not hasattr(infoComp, self._moneyName)
            or getattr(infoComp, self._moneyName) < price
            or not hasattr(infoComp, abilityKey)
        ):
            Manager.playSE(GameSystem.getBuzzerSE())
            self.refreshItems()
            return
        setattr(
            infoComp,
            self._moneyName,
            getattr(infoComp, self._moneyName) - price,
        )
        setattr(
            infoComp,
            abilityKey,
            getattr(infoComp, abilityKey) + self._abilities[abilityKey],
        )
        self._increasePrice(abilityIndex)
        Manager.playSE(GameSystem.getShopSE())
        self.refreshPriceText()
        self.refreshItems()

    def getVisible(self) -> bool:
        r"""\brief Return whether the shop is visible."""
        return self._selectable.getVisible()

    def isClosed(self) -> bool:
        r"""\brief Return whether the latest shop session has closed."""
        return self._closed

    @classmethod
    def _getDefaultRect(cls) -> Tuple[Pair[int], Pair[int]]:
        gameSize = GlobalSystem.getGameSize()
        x = int((gameSize.x - cls._SIZE) / 2)
        y = int((gameSize.y - cls._SIZE) / 2)
        return ((x, y), (cls._SIZE, cls._SIZE))

    def _refreshAvatar(self, shopActor: Optional[Actor]) -> None:
        self._avatarImage.setVisible(False)
        self._avatarTexture = None
        self._avatarRect = None
        self._avatarAnimatable = False
        self._avatarSwitchTimer = 0.0
        if shopActor is None:
            return
        texture = shopActor.getTexture()
        if texture is None:
            return
        textureRect = copy.copy(shopActor.getTextureRect())
        frameSize = textureRect.size
        if frameSize.x <= 0 or frameSize.y <= 0:
            return
        self._avatarTexture = texture
        self._avatarRect = textureRect
        self._avatarAnimatable = shopActor.getAnimatable()
        self._avatarSwitchInterval = float(shopActor.switchInterval)
        self._avatarImage.setTexture(texture, False)
        self._avatarImage.setTextureRect(textureRect)
        self._avatarImage.setScale(
            Vector2f(
                float(_AVATAR_SIZE) / float(frameSize.x),
                float(_AVATAR_SIZE) / float(frameSize.y),
            )
        )
        self._avatarImage.setVisible(True)

    def _animateAvatar(self, deltaTime: float) -> None:
        if (
            not self._avatarAnimatable
            or not self._avatarImage.getVisible()
            or self._avatarTexture is None
            or self._avatarRect is None
        ):
            return
        self._avatarSwitchTimer += deltaTime
        if self._avatarSwitchTimer < self._avatarSwitchInterval:
            return
        self._avatarSwitchTimer = 0.0
        rect = copy.copy(self._avatarRect)
        textureWidth = self._avatarTexture.getSize().x
        rect.position.x = (rect.position.x + rect.size.x) % textureWidth
        self._avatarRect = rect
        self._avatarImage.setTextureRect(rect)

    def _formatPurchaseText(
        self,
        abilityKey: str,
        delta: int,
        price: int,
        moneyDisplayName: Optional[str] = None,
    ) -> str:
        priceValue = self._getPriceValue()
        if not isinstance(priceValue, (list, tuple)):
            return f"{delta} {self.getAttributeDisplayName(abilityKey)}"
        if moneyDisplayName is None:
            moneyDisplayName = self.getAttributeDisplayName(self._moneyName)
        return f"{price} {moneyDisplayName} :  " f"{delta} {self.getAttributeDisplayName(abilityKey)}"

    def _getPriceValue(self) -> Any:
        if self._priceRef is None:
            return self._fallbackPrice
        return self._priceRef.get()

    def _setPriceValue(self, value: Any) -> None:
        if self._priceRef is None:
            self._fallbackPrice = value
            return
        self._priceRef.set(value)

    def _getPrices(self) -> List[int]:
        priceValue = self._getPriceValue()
        if isinstance(priceValue, (list, tuple)):
            if len(priceValue) != len(self._abilities):
                raise ValueError("Attribute shop price list length must match abilities")
            return [int(price) for price in priceValue]
        return [int(priceValue) for _ in self._abilities]

    def _increasePrice(self, abilityIndex: int) -> None:
        priceValue = self._getPriceValue()
        if isinstance(priceValue, (list, tuple)):
            prices = [int(price) for price in priceValue]
            prices[abilityIndex] += self._priceIncrement
            self._setPriceValue(prices)
            return
        self._setPriceValue(int(priceValue) + self._priceIncrement)

    def _closeAndNotify(self) -> None:
        self.close()
        if self._onCloseCallback is not None:
            self._onCloseCallback()
