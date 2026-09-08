local WindowAttrShopUI = require("Source.UI.WindowAttrShop")
local WindowAttrShopSelectable = require("Source.Windows.WindowAttrShop.Selectable")

---@class Source.Windows.WindowAttrShop
local WindowAttrShop = {}

local _SIZE = 352
local _ABILITY_ORDER = { "LEVEL", "ATK", "DEF", "MAXHP", "HP", "EXP", "GOLD" }
WindowAttrShop.uiClass = WindowAttrShopUI

function WindowAttrShop:_getPriceValue()
    if self._priceRef == nil then
        return self._fallbackPrice
    end
    return self._priceRef:get()
end

function WindowAttrShop:_setPriceValue(value)
    if self._priceRef == nil then
        assert(Class.isInstance(value, "number"), "Fallback price must be a number")
        ---@cast value integer
        self._fallbackPrice = math.trunc(value)
        return
    end
    self._priceRef:set(value)
end

function WindowAttrShop:_getPrices()
    local priceValue = self:_getPriceValue()
    if Class.isInstance(priceValue, "table") then
        ---@cast priceValue integer[]
        if #priceValue ~= #self._abilityKeys then
            error("Attribute shop price list length must match abilities")
        end
        ---@type integer[]
        local result = {}
        for index, price in ipairs(priceValue) do
            result[index] = math.trunc(tonumber(price) or 0)
        end
        return result
    end
    ---@type integer[]
    local result = {}
    for index = 1, #self._abilityKeys do
        result[index] = math.trunc(tonumber(priceValue) or 0)
    end
    return result
end

function WindowAttrShop:_increasePrice(abilityIndex)
    local priceValue = self:_getPriceValue()
    if Class.isInstance(priceValue, "table") then
        ---@cast priceValue integer[]
        local prices = {}
        for index, price in ipairs(priceValue) do
            prices[index] = math.trunc(tonumber(price) or 0)
        end
        prices[abilityIndex] = prices[abilityIndex] + self._priceIncrement
        self:_setPriceValue(prices)
        return
    end
    self:_setPriceValue(math.trunc(tonumber(priceValue) or 0) + self._priceIncrement)
end

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
    self._shopUI = self.uiClass.new(self)
    self._selectable = WindowAttrShopSelectable.new(WindowAttrShop.GetDefaultRect(), self, self._shopUI)
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
    self._player = player
end

function WindowAttrShop:getAttributeDisplayName(attributeName)
    return self._shopUI:getAttributeDisplayName(attributeName)
end

function WindowAttrShop:open(shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, rect)
    self._abilities = {}
    for key, value in pairs(abilities) do
        self._abilities[tostring(key)] = math.trunc(tonumber(value) or 0)
    end
    self._abilityKeys = table.orderedStringKeys(self._abilities, _ABILITY_ORDER)
    self._priceRef = priceRef
    if priceRef == nil then
        self._fallbackPrice = 0
    end
    self._priceIncrement = priceIncrement
    self._moneyName = tostring(moneyName or "GOLD")
    self:_getPrices()
    self._closed = false
    self._shopUI:open(shopActor, shopName, shopDescription, rect)
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

function WindowAttrShop:close(notify)
    self._shopUI:close(function ()
        self._closed = true
        if notify and self._onCloseCallback ~= nil then
            self._onCloseCallback()
        end
    end)
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
    return WindowAttrShopUI.GetDefaultRect(_SIZE)
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

function WindowAttrShop:getCurrencyName()
    return self._moneyName
end

function WindowAttrShop:getSharedPrice()
    local price = self:_getPriceValue()
    if Class.isInstance(price, "table") then
        return nil
    end
    return math.trunc(tonumber(price) or 0)
end

function WindowAttrShop:getOffers()
    local prices = self:_getPrices()
    local result = {}
    for index, key in ipairs(self._abilityKeys) do
        local price = assert(prices[index])
        result[index] = {
            key = key,
            delta = self._abilities[key],
            price = price,
            available = self._player.attributes[self._moneyName] ~= nil and self._player.attributes[key] ~= nil
                and self._player.attributes[self._moneyName] >= price
        }
    end
    return result
end

function WindowAttrShop:purchaseAttribute(key)
    local abilityIndex = table.index(self._abilityKeys, key)
    if abilityIndex == nil then
        return false
    end
    local price = assert(self:_getPrices()[abilityIndex])
    if self._player.attributes[self._moneyName] == nil or self._player.attributes[self._moneyName] < price
        or self._player.attributes[key] == nil then
        return false
    end
    local abilitySystem = self._player:getAbilitySystemComponent()
    local changedAttributes = { [self._moneyName] = abilitySystem:getNumericAttributeBase(self._moneyName) - price }
    changedAttributes[key] = (changedAttributes[key] or abilitySystem:getNumericAttributeBase(key))
        + self._abilities[key]
    abilitySystem:setNumericAttributeBases(changedAttributes)
    self:_increasePrice(abilityIndex)
    return true
end

return class(WindowAttrShop)
