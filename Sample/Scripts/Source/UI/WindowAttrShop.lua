local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local AttrShopRowUI = require("Source.UI.Parts.WindowAttrShop.AttrShopRow")
local Ui = require("Source.UI.Ui")
local UiLayout = require("Source.UI.UiLayout")

local ManagerFunctions = GlobalFunctions.Manager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _AVATAR_SIZE = 32
local _ITEM_ROW_HEIGHT = 32
local _ABILITY_ORDER = { "LEVEL", "ATK", "DEF", "MAXHP", "HP", "EXP", "GOLD" }

---@class Source.UI.WindowAttrShop
local WindowAttrShopUI = {}

function WindowAttrShopUI:init(model)
    super(WindowAttrShopUI, self).init(model)
    self._selectable = nil
    self._logicalSize = nil
    self._shopNameSource = ""
    self._descriptionSource = ""
    self._shopName = ""
    self._description = ""
    self._priceTextValue = ""
    self._rows = {}
end

function WindowAttrShopUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("AbilityScrollBox")
    self._listView = self:requireControl("AbilityList")
    self.model._avatarImage = self:requireControl("Avatar")
    self.model._nameText = self:requireControl("ShopName")
    self.model._descText = self:requireControl("Description")
    self.model._priceText = self:requireControl("Price")
end

function WindowAttrShopUI:refresh()
    self:setText("ShopName", self._shopName)
    self:setText("Description", self._description)
    self:setText("Price", self._priceTextValue)
    self:setProperty("Avatar", "visible", false)
end

function WindowAttrShopUI:attachSelectable(selectable, size)
    self._selectable = selectable
    local logicalSize = sf.Vector2u.new(size.x, size.y)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
    self:attachWindowView(selectable, self._logicalSize)
end

function WindowAttrShopUI:getWindowFrame()
    return self._windowFrame
end

function WindowAttrShopUI:getContent()
    return self._content
end

function WindowAttrShopUI:getListView()
    return self._listView
end

function WindowAttrShopUI:getScrollBox()
    return self._scrollBox
end

---@return Source.Windows._WindowAttrShopSelectable
function WindowAttrShopUI:_getSelectable()
    assert(self._selectable ~= nil, "Attribute shop selectable has not been attached")
    return self._selectable
end

function WindowAttrShopUI:refreshRows(abilities, prices, moneyName, moneyAmount)
    local selectable = self:_getSelectable()
    local previousIndex = selectable.index
    selectable._abilityKeys = {}
    for index, key in ipairs(self.model._abilityKeys) do
        selectable._abilityKeys[index] = key
    end
    selectable._cellAvailable = {}
    self._listView:clearChildren()
    self._rows = {}
    local cellWidth = selectable:_getRectWidth()
    local moneyDisplayName = self:getAttributeDisplayName(moneyName)
    for luaIndex, abilityKey in ipairs(selectable._abilityKeys) do
        local price = prices[luaIndex]
        ---@cast price - nil
        local delta = abilities[abilityKey]
        local attributes = self.model:getPlayer().attributes
        local available = moneyAmount >= price and attributes[moneyName] ~= nil and attributes[abilityKey] ~= nil
        selectable._cellAvailable[#selectable._cellAvailable + 1] = available
        self:_addRow(self:formatPurchaseText(abilityKey, delta, price, moneyDisplayName), available, cellWidth)
    end
    self:_addRow(LOC("SHOP_ATTR_LEAVE"), true, cellWidth)
    selectable._cellAvailable[#selectable._cellAvailable + 1] = true
    if previousIndex == nil then
        selectable.index = 0
    else
        local selectedIndex = math.min(previousIndex, #selectable._abilityKeys)
        ---@cast selectedIndex integer
        selectable.index = selectedIndex
    end
    self:_reflow()
    selectable:_detachSelectionRect()
end

---@param textValue string
---@param available boolean
---@param width     integer
function WindowAttrShopUI:_addRow(textValue, available, width)
    local row = AttrShopRowUI.new({
        text = textValue,
        available = available
    })
    local logicalSize = sf.Vector2u.new(width, _ITEM_ROW_HEIGHT)
    ---@cast logicalSize sf.Vector2u
    local cell = row:prepare(logicalSize)
    cell:addConfirmCallback(function ()
        self:confirmItem()
    end)
    self._rows[#self._rows + 1] = row
    self._listView:addChild(cell)
end

function WindowAttrShopUI:tick(deltaTime)
    self:animateAvatar(deltaTime)
end

function WindowAttrShopUI:setPlayer(player)
    self.model._player = player
end

---@diagnostic disable-next-line: unused
function WindowAttrShopUI:getAttributeDisplayName(attributeName)
    return LOC
        (attributeName)
        :gsub("%s+$", "")
        :gsub("[:：]+$", "")
end

function WindowAttrShopUI:open(
    shopActor, shopName, shopDescription, abilities, priceRef, priceIncrement, moneyName, rect
)
    local selectable = self:_getSelectable()
    self.model._abilities = {}
    for key, value in pairs(abilities) do
        self.model._abilities[tostring(key)] = Engine.ToInteger(tonumber(value) or 0)
    end
    self.model._abilityKeys = table.orderedStringKeys(self.model._abilities, _ABILITY_ORDER)
    self.model._priceRef = priceRef
    if priceRef == nil then
        self.model._fallbackPrice = 0
    end
    self.model._priceIncrement = priceIncrement
    self.model._moneyName = tostring(moneyName or "GOLD")
    self:getPrices()
    if rect ~= nil then
        selectable:setPosition(sf.Vector2f.new(rect.position.x, rect.position.y))
    end
    self:refreshAvatar(shopActor)
    self._shopNameSource = tostring(shopName or "")
    self._descriptionSource = tostring(shopDescription or "")
    self:refreshLocale()
    selectable:resetSelection()
    self.model._closed = false
    selectable:setVisible(true)
    selectable:setActive(true)
    selectable:requestKeyboardFocus()
end

function WindowAttrShopUI:refreshLocale()
    self._shopName = bool(self._shopNameSource) and LOC(self._shopNameSource) or ""
    local description = bool(self._descriptionSource) and LOC(self._descriptionSource) or ""
    self._description = description:gsub("\\n", "\n")
    self:setText("ShopName", self._shopName)
    self:setText("Description", self._description)
    self:refreshPriceText()
    self:refreshItems()
end

function WindowAttrShopUI:refreshPriceText()
    local priceValue = self:getPriceValue()
    if type(priceValue) == "table" then
        self._priceTextValue = ""
    else
        self._priceTextValue = Engine.ApplyStringMappingFormat(LOC("SHOP_ATTR_PRICE"), {
            gold = Engine.ToInteger(tonumber(priceValue) or 0)
        })
    end
    self:setText("Price", self._priceTextValue)
    self:_reflow()
end

function WindowAttrShopUI:refreshItems()
    self:refreshRows(
        self.model._abilities, self:getPrices(), self.model._moneyName,
        Engine.ToInteger(tonumber(self.model._player.attributes[self.model._moneyName]) or 0)
    )
end

function WindowAttrShopUI:close()
    local selectable = self:_getSelectable()
    selectable:setVisible(false)
    selectable:setActive(false)
    self.model._closed = true
end

function WindowAttrShopUI:closeByCancel()
    if self.model._closed then
        return
    end
    ManagerFunctions.playSE(GameSystem.GetCancelSE())
    self:closeAndNotify()
end

function WindowAttrShopUI:confirmItem()
    local selectable = self:_getSelectable()
    local abilityKey = selectable:getSelectedAbilityKey()
    if abilityKey == nil then
        self:closeByCancel()
        return
    end
    local abilityIndex = assert(
        table.index(self.model._abilityKeys, abilityKey), "Selected attribute is missing from the attribute shop model"
    )
    local price = self:getPrices()[abilityIndex]
    ---@cast price - nil
    local player = self.model:getPlayer()
    if not selectable:isCurrentAvailable() or player.attributes[self.model._moneyName] == nil
        or player.attributes[self.model._moneyName] < price or player.attributes[abilityKey] == nil then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        self:refreshItems()
        return
    end
    local abilitySystem = player:getAbilitySystemComponent()
    local changedAttributes = {
        [self.model._moneyName] = abilitySystem:getNumericAttributeBase(self.model._moneyName) - price
    }
    changedAttributes[abilityKey] = (changedAttributes[abilityKey] or abilitySystem:getNumericAttributeBase(abilityKey))
        + self.model._abilities[abilityKey]
    abilitySystem:setNumericAttributeBases(changedAttributes)
    self:increasePrice(abilityIndex)
    ManagerFunctions.playSE(GameSystem.GetShopSE())
    self:refreshPriceText()
    self:refreshItems()
end

function WindowAttrShopUI:refreshAvatar(shopActor)
    self:setProperty("Avatar", "visible", false)
    self.model._avatarTexture = nil
    self.model._avatarRect = nil
    self.model._avatarAnimatable = false
    self.model._avatarSwitchTimer = 0.0
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
    local textureRect = copy(sourceRect)
    local frameSize = textureRect.size
    if frameSize.x <= 0 or frameSize.y <= 0 then
        self:_reflow()
        return
    end
    self.model._avatarTexture = texture
    self.model._avatarRect = textureRect
    self.model._avatarAnimatable = shopActor:getAnimatable()
    self.model._avatarSwitchInterval = shopActor.switchInterval
    self.model._avatarImage:setTexture(texture, false)
    self.model._avatarImage:setTextureRect(textureRect)
    self:setProperty("Avatar", "visible", true)
    self:_reflow()
end

function WindowAttrShopUI:animateAvatar(deltaTime)
    if not self.model._avatarAnimatable or not self.model._avatarImage:getVisible()
        or self.model._avatarTexture == nil or self.model._avatarRect == nil then
        return
    end
    self.model._avatarSwitchTimer = self.model._avatarSwitchTimer + deltaTime
    if self.model._avatarSwitchTimer < self.model._avatarSwitchInterval then
        return
    end
    self.model._avatarSwitchTimer = 0.0
    local textureWidth = self.model._avatarTexture:getSize().x
    local positionX = (self.model._avatarRect.position.x + self.model._avatarRect.size.x) % textureWidth
    ---@cast positionX integer
    local avatarRect = sf.IntRect.new(
        positionX, self.model._avatarRect.position.y, self.model._avatarRect.size.x, self.model._avatarRect.size.y
    )
    ---@cast avatarRect sf.IntRect
    self.model._avatarRect = avatarRect
    self.model._avatarImage:setTextureRect(self.model._avatarRect)
end

function WindowAttrShopUI:formatPurchaseText(abilityKey, delta, price, moneyDisplayName)
    local priceValue = self:getPriceValue()
    if type(priceValue) ~= "table" then
        return tostring(delta) .. " " .. self:getAttributeDisplayName(abilityKey)
    end
    moneyDisplayName = moneyDisplayName or self:getAttributeDisplayName(self.model._moneyName)
    return tostring(price) .. " " .. moneyDisplayName .. " :  " .. tostring(delta) .. " "
        .. self:getAttributeDisplayName(abilityKey)
end

function WindowAttrShopUI:getPriceValue()
    if self.model._priceRef == nil then
        return self.model._fallbackPrice
    end
    return self.model._priceRef:get()
end

function WindowAttrShopUI:setPriceValue(value)
    if self.model._priceRef == nil then
        self.model._fallbackPrice = Engine.ToInteger(value)
        return
    end
    self.model._priceRef:set(value)
end

function WindowAttrShopUI:getPrices()
    local priceValue = self:getPriceValue()
    if type(priceValue) == "table" then
        if #priceValue ~= #self.model._abilityKeys then
            error("Attribute shop price list length must match abilities")
        end
        local result = {}
        for index, price in ipairs(priceValue) do
            result[index] = Engine.ToInteger(tonumber(price) or 0)
        end
        return result
    end
    local result = {}
    for index = 1, #self.model._abilityKeys do
        result[index] = Engine.ToInteger(tonumber(priceValue) or 0)
    end
    return result
end

function WindowAttrShopUI:increasePrice(abilityIndex)
    local priceValue = self:getPriceValue()
    if type(priceValue) == "table" then
        local prices = {}
        for index, price in ipairs(priceValue) do
            prices[index] = Engine.ToInteger(tonumber(price) or 0)
        end
        prices[abilityIndex] = prices[abilityIndex] + self.model._priceIncrement
        self:setPriceValue(prices)
        return
    end
    self:setPriceValue(Engine.ToInteger(tonumber(priceValue) or 0) + self.model._priceIncrement)
end

function WindowAttrShopUI:closeAndNotify()
    self:close()
    if self.model._onCloseCallback ~= nil then
        self.model._onCloseCallback()
    end
end

function WindowAttrShopUI.GetDefaultRect(size)
    return UiLayout.GetCenteredRect(size, size)
end

function WindowAttrShopUI:_reflow()
    self.view:reflow(self._logicalSize)
    if self.model._avatarRect == nil then
        return
    end
    self.model._avatarImage:setScale(
        sf.Vector2f.new(_AVATAR_SIZE / self.model._avatarRect.size.x, _AVATAR_SIZE / self.model._avatarRect.size.y)
    )
end

return Ui.Define("WindowAttrShop", WindowAttrShopUI)
