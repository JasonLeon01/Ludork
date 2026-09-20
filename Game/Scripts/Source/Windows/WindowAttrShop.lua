local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local AttrShopRowController = require("Source.Windows.WindowAttrShop.AttrShopRow.Controller")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowAttrShop")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local AudioManager = GlobalCore.AudioManager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _ABILITY_ORDER = { "LEVEL", "ATK", "DEF", "MAXHP", "HP", "EXP", "GOLD" }
local _AVATAR_SIZE = 32
local _ITEM_ROW_HEIGHT = 32

---@class Source.Windows.WindowAttrShop.Controller
local Controller = {}

Controller.windowOptions = {
    centered = true,
    hidden = true,
    returnButton = true,
    list = "AbilityList",
    scroll = "AbilityScrollBox",
    itemHeight = _ITEM_ROW_HEIGHT
}

function Controller:refresh()
    self:setText("ShopName", self._shopName)
    self:setText("Description", self._description)
    self:setText("Price", self._priceTextValue)
    self:setProperty("Avatar", "visible", false)
end

function Controller:refreshRows()
    self._offers = self:getOffers()
    self._rows:clear()
    local cellWidth = self.host:getItemWidth()
    local moneyDisplayName = self:getAttributeDisplayName(self:getCurrencyName())
    for _, offer in ipairs(self._offers) do
        self:_addRow(
            self:formatPurchaseText(offer.key, offer.delta, offer.price, moneyDisplayName), offer.available, cellWidth
        )
    end
    self:_addRow(LOC("SHOP_ATTR_LEAVE"), true, cellWidth)
    self._rows:layout()
    if self.host.index == nil then
        self.host.index = 0
    else
        self.host.index = math.trunc(math.min(self.host.index, #self._offers))
    end
    self:_reflow()
    self.host:detachSelectionRect()
end

function Controller:getSelectedAbilityKey()
    local offer = self.host.index ~= nil and self._offers[self.host.index + 1] or nil
    return offer ~= nil and offer.key or nil
end

function Controller:isCurrentAvailable()
    if self.host.index == nil or self.host.index < 0 or self.host.index > #self._offers then
        return false
    end
    return self.host.index == #self._offers or assert(self._offers[self.host.index + 1]).available
end

---@param textValue string
---@param available boolean
---@param width     integer
function Controller:_addRow(textValue, available, width)
    local logicalSize = sf.Vector2u.new(width, _ITEM_ROW_HEIGHT)
    ---@cast logicalSize sf.Vector2u
    local row = self._rows:add({
        text = textValue,
        available = available
    }, logicalSize)
    local cell = row.ui.root
    cell:addConfirmCallback(self:bindCallback(Controller.confirmItem))
end

---@diagnostic disable-next-line: unused
function Controller:getAttributeDisplayName(attributeName)
    return LOC
        (attributeName)
        :gsub("%s+$", "")
        :gsub("[:：]+$", "")
end

function Controller:open(shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName)
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
    self:refreshAvatar(shopActor)
    self._shopNameSource = tostring(shopName or "")
    self._descriptionSource = tostring(shopDescription or "")
    self:_refreshLocale()
    self.host:resetSelection()
    self.host:showWithAnimation("FadeIn", function ()
        self.host:setActive(true)
        self.host:requestKeyboardFocus()
    end)
end

function Controller:_refreshLocale()
    self._shopName = bool(self._shopNameSource) and LOC(self._shopNameSource) or ""
    local description = bool(self._descriptionSource) and LOC(self._descriptionSource) or ""
    self._description = description:gsub("\\n", "\n")
    self:setText("ShopName", self._shopName)
    self:setText("Description", self._description)
    self:refreshPriceText()
    self:refreshItems()
end

function Controller:refreshPriceText()
    local priceValue = self:getSharedPrice()
    if priceValue == nil then
        self._priceTextValue = ""
    else
        self._priceTextValue = Engine.ApplyStringMappingFormat(LOC("SHOP_ATTR_PRICE"), {
            gold = math.trunc(tonumber(priceValue) or 0)
        })
    end
    self:setText("Price", self._priceTextValue)
    self:_reflow()
end

function Controller:refreshItems()
    self:refreshRows()
end

function Controller:close(notify)
    self.host:setActive(false)
    self.host:hideWithAnimation("FadeOut", function ()
        self._closed = true
        if notify and self._onCloseCallback ~= nil then
            self._onCloseCallback()
        end
    end)
end

function Controller:closeByCancel()
    if self:isClosed() then
        return
    end
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close(true)
end

function Controller:confirmItem()
    local abilityKey = self:getSelectedAbilityKey()
    if abilityKey == nil then
        self:closeByCancel()
        return
    end
    if not self:purchaseAttribute(abilityKey) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        self:refreshItems()
        return
    end
    AudioManager.playSound(GameSystem.GetShopSE())
    self:refreshPriceText()
    self:refreshItems()
end

function Controller:refreshAvatar(shopActor)
    self:setProperty("Avatar", "visible", false)
    self._avatarTexture = nil
    self._avatarRect = nil
    self._avatarAnimatable = false
    self._avatarSwitchTimer = 0.0
    if shopActor == nil then
        self:_reflow()
        return
    end
    local texture = shopActor:getTexture()
    if texture == nil then
        self:_reflow()
        return
    end
    local sourceRect = shopActor:getTextureRect()
    local textureRect = sourceRect:copy()
    local frameSize = textureRect.size
    if frameSize.x <= 0 or frameSize.y <= 0 then
        self:_reflow()
        return
    end
    self._avatarTexture = texture
    self._avatarRect = textureRect
    self._avatarAnimatable = shopActor:getAnimatable()
    self._avatarSwitchInterval = shopActor.switchInterval
    self.ui.controls["Avatar"]:setTexture(texture, false)
    self.ui.controls["Avatar"]:setTextureRect(textureRect)
    self:setProperty("Avatar", "visible", true)
    self:_reflow()
end

function Controller:animateAvatar(deltaTime)
    if not self._avatarAnimatable or not self.ui.controls["Avatar"]:getVisible()
        or self._avatarTexture == nil or self._avatarRect == nil then
        return
    end
    self._avatarSwitchTimer = self._avatarSwitchTimer + deltaTime
    if self._avatarSwitchTimer < self._avatarSwitchInterval then
        return
    end
    self._avatarSwitchTimer = 0.0
    local textureWidth = self._avatarTexture:getSize().x
    local positionX = (self._avatarRect.position.x + self._avatarRect.size.x) % textureWidth
    ---@cast positionX integer
    local avatarRect = sf.IntRect.new(
        positionX, self._avatarRect.position.y, self._avatarRect.size.x, self._avatarRect.size.y
    )
    ---@cast avatarRect sf.IntRect
    self._avatarRect = avatarRect
    self.ui.controls["Avatar"]:setTextureRect(self._avatarRect)
end

function Controller:formatPurchaseText(abilityKey, delta, price, moneyDisplayName)
    local priceValue = self:getSharedPrice()
    if priceValue ~= nil then
        return tostring(delta) .. " " .. self:getAttributeDisplayName(abilityKey)
    end
    moneyDisplayName = moneyDisplayName or self:getAttributeDisplayName(self:getCurrencyName())
    return tostring(price) .. " " .. moneyDisplayName .. " :  " .. tostring(delta) .. " "
        .. self:getAttributeDisplayName(abilityKey)
end

function Controller:_reflow()
    self.view:reflow(self._logicalSize)
    if self._avatarRect == nil then
        return
    end
    self.ui.controls["Avatar"]:setScale(
        sf.Vector2f.new(_AVATAR_SIZE / self._avatarRect.size.x, _AVATAR_SIZE / self._avatarRect.size.y)
    )
end

function Controller:init(player, onClose)
    self._player = player
    self._onCloseCallback = onClose
    self._abilities = {}
    self._abilityKeys = {}
    self._priceRef = nil
    self._fallbackPrice = 0
    self._priceIncrement = 1
    self._moneyName = "GOLD"
    self._closed = true
    local size = self.host:getSize()
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
    self._shopNameSource = ""
    self._descriptionSource = ""
    self._shopName = ""
    self._description = ""
    self._priceTextValue = ""
    self._offers = {}
    self._avatarTexture = nil
    self._avatarRect = nil
    self._avatarAnimatable = false
    self._avatarSwitchInterval = 0.2
    self._avatarSwitchTimer = 0.0
    self._rows = self:createCollection(self.ui.controls["AbilityList"], AttrShopRowController)
end

function Controller:_getPriceValue()
    if self._priceRef == nil then
        return self._fallbackPrice
    end
    return self._priceRef:get()
end

function Controller:_setPriceValue(value)
    if self._priceRef == nil then
        assert(Class.isInstance(value, "number"), "Fallback price must be a number")
        ---@cast value integer
        self._fallbackPrice = math.trunc(value)
        return
    end
    self._priceRef:set(value)
end

function Controller:_getPrices()
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

function Controller:_increasePrice(abilityIndex)
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

function Controller:getPlayer()
    return self._player
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:getCurrencyName()
    return self._moneyName
end

function Controller:getSharedPrice()
    local price = self:_getPriceValue()
    if Class.isInstance(price, "table") then
        return nil
    end
    return math.trunc(tonumber(price) or 0)
end

function Controller:getOffers()
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

function Controller:purchaseAttribute(key)
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
    local changedAttributes = { [self._moneyName] = self._player:getAttr(self._moneyName, true) - price }
    changedAttributes[key] = (changedAttributes[key] or self._player:getAttr(key, true)) + self._abilities[key]
    abilitySystem:setNumericAttributeBases(changedAttributes)
    self:_increasePrice(abilityIndex)
    return true
end

function Controller:isClosed()
    return self._closed
end

function Controller:refreshLocale()
    if self.host:getVisible() then
        self:_refreshLocale()
    end
end

function Controller:onTick(deltaTime)
    self:animateAvatar(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
end

function Controller:onReturn()
    self:closeByCancel()
end

function Controller:dispose()
    self._onCloseCallback = nil
    super(Controller, self).dispose()
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
