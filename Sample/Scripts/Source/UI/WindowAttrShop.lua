local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")
local UiControlFactory = require("Source.UI.UiControlFactory")
local UiLayout = require("Source.UI.UiLayout")

local Input = Engine.Input
local ManagerFunctions = GlobalFunctions.Manager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _AVATAR_SIZE = 32
local _ITEM_ROW_HEIGHT = 32
local _DISABLED_COLOUR = sf.Color.new(160, 160, 160, 255)
local _ENABLED_COLOUR = sf.Color.new(255, 255, 255, 255)
local _ABILITY_ORDER = { "LEVEL", "ATK", "DEF", "MAXHP", "HP", "EXP", "GOLD" }

---@class Source.UI.WindowAttrShop.AttrShopRow
local AttrShopRow = {}

function AttrShopRow:init(model)
    self.model = model
    self.root, self._label = UiControlFactory.createFunctionalTextRow(
        sf.Vector2u.new(1, 1), Data.getPlainTextConfig("UI/CenterText22")
    )
end

function AttrShopRow:refresh()
    self._label:setString(self.model.text)
    self._label:setColour(self.model.available and _ENABLED_COLOUR or _DISABLED_COLOUR)
    UiControlFactory.layoutCenteredTextRow(self.root, self._label, 8.0)
end

function AttrShopRow:prepare(logicalSize)
    self.root:resize(logicalSize)
    self:refresh()
    return self.root
end

local FinalAttrShopRow = class(AttrShopRow)

---@class Source.UI.WindowAttrShop
local WindowAttrShopUI = {}

function WindowAttrShopUI:init(model)
    super(WindowAttrShopUI, self).init(model)
    self._selectable = nil
    self._logicalSize = nil
    self._shopName = ""
    self._description = ""
    self._priceTextValue = ""
    self._rows = {}
end

function WindowAttrShopUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
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

function WindowAttrShopUI:prepareSelectable(selectable, size)
    self._selectable = selectable
    self._logicalSize = sf.Vector2u.new(size.x, size.y)
    return self:prepare(self._logicalSize)
end

function WindowAttrShopUI:attachSelectable(selectable, size)
    local root = self:prepareSelectable(selectable, size)
    self:_attachWindowRoot(selectable, root)
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
        local infoComp = self.model:getPlayer().infoComp
        local available = moneyAmount >= price and infoComp[moneyName] ~= nil and infoComp[abilityKey] ~= nil
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
    if selectable._rect:getParent() ~= nil then
        selectable.content:removeChild(selectable._rect)
    end
end

---@param textValue string
---@param available boolean
---@param width     integer
function WindowAttrShopUI:_addRow(textValue, available, width)
    local row = FinalAttrShopRow.new({
        text = textValue,
        available = available
    })
    local cell = row:prepare(sf.Vector2u.new(width, _ITEM_ROW_HEIGHT))
    cell:addConfirmCallback(function ()
        self:confirmItem()
    end)
    self._rows[#self._rows + 1] = row
    self._listView:addChild(cell)
end

function WindowAttrShopUI:tick(deltaTime)
    self:animateAvatar(deltaTime)
end

function WindowAttrShopUI:handleKeyDown()
    if not Input.isActionTriggered(Input.getCancelKeys(), false) then
        return false
    end
    self:closeByCancel()
    Input.isActionTriggered(Input.getCancelKeys(), true)
    return true
end

function WindowAttrShopUI:handleMouseButtonDown(kwargs)
    if kwargs.button ~= sf.Mouse.Button.Right then
        return false
    end
    self:closeByCancel()
    return true
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
    self.model._priceIncrement = Engine.ToInteger(priceIncrement)
    self.model._moneyName = tostring(moneyName or "GOLD")
    self:getPrices()
    if rect ~= nil then
        selectable:setPosition(sf.Vector2f.new(rect.position.x, rect.position.y))
    end
    self:refreshAvatar(shopActor)
    self._shopName = bool(shopName) and LOC(shopName) or ""
    local description = bool(shopDescription) and LOC(shopDescription) or ""
    self._description = description:gsub("\\n", "\n")
    self:setText("ShopName", self._shopName)
    self:setText("Description", self._description)
    self:refreshPriceText()
    self:refreshItems()
    self.model._closed = false
    selectable:setVisible(true)
    selectable:setActive(true)
    selectable:requestKeyboardFocus()
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
        Engine.ToInteger(tonumber(self.model._player.infoComp[self.model._moneyName]) or 0)
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
    ManagerFunctions.playSE(GameSystem.getCancelSE())
    self:closeAndNotify()
end

function WindowAttrShopUI:confirmItem()
    local selectable = self:_getSelectable()
    local abilityKey = selectable:getSelectedAbilityKey()
    if abilityKey == nil then
        self:closeByCancel()
        return
    end
    ---@type integer | nil
    local abilityIndex = nil
    for index, key in ipairs(self.model._abilityKeys) do
        if key == abilityKey then
            abilityIndex = index
            break
        end
    end
    assert(abilityIndex ~= nil, "Selected attribute is missing from the attribute shop model")
    ---@cast abilityIndex - nil
    local price = self:getPrices()[abilityIndex]
    ---@cast price - nil
    local infoComp = self.model._player.infoComp
    if not selectable:isCurrentAvailable() or infoComp[self.model._moneyName] == nil
        or infoComp[self.model._moneyName] < price or infoComp[abilityKey] == nil then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        self:refreshItems()
        return
    end
    infoComp[self.model._moneyName] = infoComp[self.model._moneyName] - price
    infoComp[abilityKey] = infoComp[abilityKey] + self.model._abilities[abilityKey]
    self:increasePrice(abilityIndex)
    ManagerFunctions.playSE(GameSystem.getShopSE())
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
    self.model._avatarSwitchInterval = tonumber(shopActor.switchInterval) or 0.2
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
    self.model._avatarRect = sf.IntRect.new(
        sf.Vector2i.new(positionX, self.model._avatarRect.position.y),
        sf.Vector2i.new(self.model._avatarRect.size.x, self.model._avatarRect.size.y)
    )
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
    local frameSize = self.model._avatarRect.size
    self.model._avatarImage:setScale(sf.Vector2f.new(_AVATAR_SIZE / frameSize.x, _AVATAR_SIZE / frameSize.y))
end

return Ui.define("WindowAttrShop", WindowAttrShopUI)
