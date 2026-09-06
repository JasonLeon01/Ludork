local WindowAttrShopUI = require("Source.UI.WindowAttrShop")
local WindowAttrShopSelectable = require("Source.Windows.WindowAttrShop.Selectable")

---@class Source.Windows.WindowAttrShop
local WindowAttrShop = {}

WindowAttrShop._SIZE = 352
WindowAttrShop.uiClass = WindowAttrShopUI

function WindowAttrShop:init(player, onClose)
    self._player = player
    self._onCloseCallback = onClose
    self._abilities = {}
    self._abilityKeys = {}
    self._priceRef = nil
    self._fallbackPrice = 0
    self._priceIncrement = 1
    self._moneyName = "GOLD"
    self._closed = true
    self._avatarTexture = nil
    self._avatarRect = nil
    self._avatarAnimatable = false
    self._avatarSwitchInterval = 0.2
    self._avatarSwitchTimer = 0.0
    self._shopUI = self.uiClass.new(self)
    self._selectable = WindowAttrShopSelectable.new(WindowAttrShop.GetDefaultRect(), self)
    self._selectable:hideImmediate()
    self._closed = true
end

function WindowAttrShop:getSelectable()
    return self._selectable
end

function WindowAttrShop:getPlayer()
    return self._player
end

function WindowAttrShop:setPlayer(player)
    self._shopUI:setPlayer(player)
end

function WindowAttrShop:getAttributeDisplayName(attributeName)
    return self._shopUI:getAttributeDisplayName(attributeName)
end

function WindowAttrShop:open(shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, rect)
    self._shopUI:open(shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, rect)
end

function WindowAttrShop:refreshPriceText()
    self._shopUI:refreshPriceText()
end

function WindowAttrShop:refreshItems()
    self._shopUI:refreshItems()
end

function WindowAttrShop:refreshLocale()
    if not self:getVisible() then
        return
    end
    self._shopUI:refreshLocale()
end

function WindowAttrShop:close()
    self._shopUI:close()
end

function WindowAttrShop:closeByCancel()
    self._shopUI:closeByCancel()
end

function WindowAttrShop:confirmItem()
    self._shopUI:confirmItem()
end

function WindowAttrShop:getVisible()
    return self._selectable:getVisible()
end

function WindowAttrShop:isClosed()
    return self._closed
end

---@return sf.IntRect
function WindowAttrShop.GetDefaultRect()
    return WindowAttrShopUI.GetDefaultRect(WindowAttrShop._SIZE)
end

---@param shopActor Engine.Actor | nil
function WindowAttrShop:_refreshAvatar(shopActor)
    self._shopUI:refreshAvatar(shopActor)
end

function WindowAttrShop:animateAvatar(deltaTime)
    self._shopUI:animateAvatar(deltaTime)
end

function WindowAttrShop:formatPurchaseText(abilityKey, delta, price, moneyDisplayName)
    return self._shopUI:formatPurchaseText(abilityKey, delta, price, moneyDisplayName)
end

---@return integer | integer[]
function WindowAttrShop:_getPriceValue()
    return self._shopUI:getPriceValue()
end

---@param value integer | integer[]
function WindowAttrShop:_setPriceValue(value)
    self._shopUI:setPriceValue(value)
end

---@return table
function WindowAttrShop:_getPrices()
    return self._shopUI:getPrices()
end

---@param abilityIndex integer
function WindowAttrShop:_increasePrice(abilityIndex)
    self._shopUI:increasePrice(abilityIndex)
end

function WindowAttrShop:_closeAndNotify()
    self._shopUI:closeAndNotify()
end

return class(WindowAttrShop)
