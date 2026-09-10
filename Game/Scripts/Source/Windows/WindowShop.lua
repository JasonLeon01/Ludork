local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowShop")
local UiLayout = require("Source.UIBase.UiLayout")
local WindowShopDetail = require("Source.Windows.WindowShopDetail")
local WindowShopItem = require("Source.Windows.WindowShopItem")
local WindowShopTabs = require("Source.Windows.WindowShopTabs")

local AudioManager = GlobalCore.AudioManager
local Canvas = Engine.Canvas

---@class Source.Windows.WindowShop.Controller
local Controller = {}

Controller.windowOptions = { centered = true, hidden = true }

Controller.SHOP_MODE_BUY = "buy"
Controller.SHOP_MODE_SELL = "sell"

function Controller:init(player, onClose)
    self._player = player
    self._onCloseCallback = onClose
    self._tabWindow = self:createChild("TabsAsset", WindowShopTabs, self.host)
    self._itemWindow = self:createChild("ItemAsset", WindowShopItem, self.host)
    self._detailWindow = self:createChild("DetailAsset", WindowShopDetail)
    self._topLeft = self.host:getPosition()
    self._tabTopLeft = self._tabWindow:getPosition()
    self._itemTopLeft = self._itemWindow:getPosition()
    self._detailTopLeft = self._detailWindow:getPosition()
    self._buyItemIDs = {}
    self._canSell = true
    self._mode = self.SHOP_MODE_BUY
    self._closed = true
end

function Controller:getTabWindow()
    return self._tabWindow
end

function Controller:getItemWindow()
    return self._itemWindow
end

function Controller:getDetailWindow()
    return self._detailWindow
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:getVisible()
    return self._transition:isBlocking()
end

function Controller:isClosed()
    return self._closed
end

function Controller:open(buyItemIDs, canSell)
    self._buyItemIDs = Controller.NormalizeBuyItems(buyItemIDs)
    self._canSell = bool(canSell)
    self._mode = self.SHOP_MODE_BUY
    self._closed = false
    self.ui.assets["TabsAsset"].controls["Tabs"]:setSelectedIndex(0)
    self:_refreshItems()
    self._itemWindow:resetSelection()
    self:_refreshDetail()
    if self._canSell then
        self.host:setPosition(self._topLeft)
        self._tabWindow:setPosition(self._tabTopLeft)
        self._itemWindow:setPosition(self._itemTopLeft)
        self._detailWindow:setPosition(self._detailTopLeft)
        self._tabWindow:setVisible(true)
        self._tabWindow:setActive(true)
    else
        local itemSize = self._itemWindow:getSize()
        local detailSize = self._detailWindow:getSize()
        local bounds = UiLayout.GetCenteredRect(itemSize.x, itemSize.y + detailSize.y)
        self.host:setPosition(sf.Vector2f.new(bounds.position.x, bounds.position.y))
        self._itemWindow:setPosition(sf.Vector2f.new(0.0, 0.0))
        self._detailWindow:setPosition(sf.Vector2f.new(0.0, itemSize.y))
        self._tabWindow:setVisible(false)
        self._tabWindow:setActive(false)
    end
    self._itemWindow:setVisible(true)
    self._itemWindow:setActive(false)
    self._detailWindow:setVisible(true)
    self._detailWindow:setActive(false)
    self._transition:show("FadeIn", function ()
        self.host:setActive(true)
        if self._canSell then
            self._tabWindow:setActive(true)
        end
        self._itemWindow:setActive(true)
        self._itemWindow:requestKeyboardFocusAtCursor()
    end)
end

function Controller:close(onHidden)
    self._tabWindow:setActive(false)
    self._itemWindow:setActive(false)
    self._detailWindow:setActive(false)
    self.host:setActive(false)
    self._transition:hide("FadeOut", function ()
        self._tabWindow:setVisible(false)
        self._itemWindow:setVisible(false)
        self._detailWindow:setVisible(false)
        self._closed = true
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function Controller:closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:_closeAndNotify()
end

function Controller:handleTabNavigationInput()
    if not self._tabWindow:getVisible() then
        return false
    end
    return self._tabWindow:handleNavigationInput()
end

---@param index integer
function Controller:onTabSelected(index)
    self:setMode(index == 1 and self.SHOP_MODE_SELL or self.SHOP_MODE_BUY)
end

function Controller:setMode(mode)
    if mode ~= self.SHOP_MODE_BUY and mode ~= self.SHOP_MODE_SELL then
        return
    end
    if not self._canSell and mode == self.SHOP_MODE_SELL then
        return
    end
    if self._mode == mode then
        return
    end
    self._mode = mode
    self:_refreshItems()
    self._itemWindow:resetSelection()
    self:_refreshDetail()
    self._itemWindow:requestKeyboardFocus()
end

function Controller:notifyItemIndexMaybeChanged()
    self:_refreshDetail()
end

function Controller:refreshLocale()
    self._tabWindow:refresh()
    self._detailWindow:refresh()
end

function Controller:confirmItem()
    local itemID = self._itemWindow:getCurrentItemID()
    if itemID == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    if self._mode == self.SHOP_MODE_BUY then
        self:_buyItem(itemID)
    else
        self:_sellItem(itemID)
    end
end

---@param buyItemIDs table
---@return table
function Controller.NormalizeBuyItems(buyItemIDs)
    local itemData = Data.GetAllGeneralItemData()
    local result = {}
    local included = {}
    for _, itemID in ipairs(buyItemIDs) do
        local itemKey = tostring(itemID)
        if itemData[itemKey] ~= nil and not included[itemKey] then
            result[#result + 1] = itemKey
            included[itemKey] = true
        end
    end
    return result
end

function Controller:_refreshItems()
    local itemIDs = nil
    local availableMap = {}
    local valueMap = {}
    local showValues = self._mode == self.SHOP_MODE_SELL
    if self._mode == self.SHOP_MODE_BUY then
        itemIDs = self._buyItemIDs
        for _, itemID in ipairs(itemIDs) do
            availableMap[itemID] = self:_canBuy(itemID)
        end
    else
        itemIDs = self:_getSellableItems()
        for _, itemID in ipairs(itemIDs) do
            availableMap[itemID] = true
            valueMap[itemID] = self._player:getItemCount(itemID)
        end
    end
    self._itemWindow:refreshItems(itemIDs, availableMap, valueMap, showValues)
    self:_refreshDetail()
end

function Controller:_refreshDetail()
    local itemID = self._itemWindow:getCurrentItemID()
    if itemID == nil then
        self._detailWindow:setItem(nil, nil)
        return
    end
    local price = self._mode == self.SHOP_MODE_BUY and Controller.GetItemPrice(itemID)
        or Controller.GetSellPrice(itemID)
    self._detailWindow:setItem(Data.GetGeneralItemData(itemID), price)
end

---@return table
function Controller:_getSellableItems()
    local itemData = Data.GetAllGeneralItemData()
    local playerItems = self._player:getItems()
    local result = {}
    for _, itemID in ipairs(table.orderedStringKeys(itemData)) do
        if (playerItems[itemID] or 0) > 0 and Controller.GetSellPrice(itemID) > 0 then
            result[#result + 1] = itemID
        end
    end
    return result
end

---@param itemID string
---@return integer
function Controller.GetItemPrice(itemID)
    local itemInfo = Data.GetGeneralItemData(itemID)
    return itemInfo.price
end

---@param itemID string
---@return integer
function Controller.GetSellPrice(itemID)
    return math.floor(Controller.GetItemPrice(itemID) / 2)
end

---@param itemID string
---@return boolean
function Controller:_canBuy(itemID)
    return self._player.attributes.GOLD >= Controller.GetItemPrice(itemID)
end

---@param itemID string
function Controller:_buyItem(itemID)
    local price = Controller.GetItemPrice(itemID)
    if not self._itemWindow:isCurrentAvailable() or self._player.attributes.GOLD < price then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        self:_refreshItems()
        return
    end
    local abilitySystem = self._player:getAbilitySystemComponent()
    abilitySystem:setNumericAttributeBase("GOLD", abilitySystem:getNumericAttributeBase("GOLD") - price)
    self._player:addItem(itemID, 1)
    AudioManager.playSound(GameSystem.GetShopSE())
    self:_refreshItems()
end

---@param itemID string
function Controller:_sellItem(itemID)
    local price = Controller.GetSellPrice(itemID)
    if price <= 0 or not self._player:removeItem(itemID, 1) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        self:_refreshItems()
        return
    end
    local abilitySystem = self._player:getAbilitySystemComponent()
    abilitySystem:setNumericAttributeBase("GOLD", abilitySystem:getNumericAttributeBase("GOLD") + price)
    AudioManager.playSound(GameSystem.GetShopSE())
    self:_refreshItems()
end

function Controller:_closeAndNotify()
    self:close(function ()
        if self._onCloseCallback ~= nil then
            self._onCloseCallback()
        end
    end)
end

function Controller:dispose()
    self._transition:hideImmediate()
    self._tabWindow:setVisible(false)
    self._itemWindow:setVisible(false)
    self._detailWindow:setVisible(false)
    self._player = nil
    self._onCloseCallback = nil
    super(Controller, self).dispose()
end

return Ui.DefineWindow(View, Controller, Canvas)
