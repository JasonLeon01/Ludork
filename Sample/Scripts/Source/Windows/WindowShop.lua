local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local WindowShopUI = require("Source.UI.WindowShop")
local UiLayout = require("Source.UI.UiLayout")
local WindowShopDetail = require("Source.Windows.WindowShopDetail")
local WindowShopItem = require("Source.Windows.WindowShopItem")
local WindowShopTabs = require("Source.Windows.WindowShopTabs")

local ManagerFunctions = GlobalFunctions.Manager
local Canvas = Engine.Canvas

local _SHOP_TAB_HEIGHT = 64
local _SHOP_ITEM_HEIGHT = 256
local _SHOP_DETAIL_HEIGHT = 96
local _SHOP_WIDTH = 352

---@class Source.Windows.WindowShop
local WindowShop = {}

WindowShop.SHOP_MODE_BUY = "buy"
WindowShop.SHOP_MODE_SELL = "sell"

function WindowShop.GetDefaultRects()
    local totalHeight = _SHOP_TAB_HEIGHT + _SHOP_ITEM_HEIGHT + _SHOP_DETAIL_HEIGHT
    local bounds = UiLayout.GetCenteredRect(_SHOP_WIDTH, totalHeight)
    return Engine.ToIntRect(bounds.position.x, bounds.position.y, _SHOP_WIDTH, _SHOP_TAB_HEIGHT),
        Engine.ToIntRect(bounds.position.x, bounds.position.y + _SHOP_TAB_HEIGHT, _SHOP_WIDTH, _SHOP_ITEM_HEIGHT),
        Engine.ToIntRect(
            bounds.position.x, bounds.position.y + _SHOP_TAB_HEIGHT + _SHOP_ITEM_HEIGHT, _SHOP_WIDTH,
            _SHOP_DETAIL_HEIGHT
        )
end

function WindowShop:init(player, tabRect, itemRect, detailRect, onClose)
    if tabRect == nil or itemRect == nil or detailRect == nil then
        local defaultTabRect, defaultItemRect, defaultDetailRect = WindowShop.GetDefaultRects()
        tabRect = tabRect or defaultTabRect
        itemRect = itemRect or defaultItemRect
        detailRect = detailRect or defaultDetailRect
    end
    local totalHeight = _SHOP_TAB_HEIGHT + _SHOP_ITEM_HEIGHT + _SHOP_DETAIL_HEIGHT
    super(WindowShop, self).init(Engine.ToIntRect(tabRect.position.x, tabRect.position.y, _SHOP_WIDTH, totalHeight))
    self._player = player
    self._onCloseCallback = onClose
    self._ui = WindowShopUI.new(self)
    self._ui:attach()
    self._transition = self._ui:createTransition(self)
    self._tabWindow = WindowShopTabs.new(
        Engine.ToIntRect(0, 0, _SHOP_WIDTH, _SHOP_TAB_HEIGHT), self, self._ui:getTabsAsset()
    )
    self._itemWindow = WindowShopItem.new(
        Engine.ToIntRect(0, _SHOP_TAB_HEIGHT, _SHOP_WIDTH, _SHOP_ITEM_HEIGHT), self, self._ui:getItemAsset()
    )
    self._detailWindow = WindowShopDetail.new(
        Engine.ToIntRect(0, _SHOP_TAB_HEIGHT + _SHOP_ITEM_HEIGHT, _SHOP_WIDTH, _SHOP_DETAIL_HEIGHT),
        self._ui:getDetailAsset()
    )
    self:addChild(self._tabWindow)
    self:addChild(self._itemWindow)
    self:addChild(self._detailWindow)
    self._topLeft = sf.Vector2f.new(tabRect.position.x, tabRect.position.y)
    self._tabTopLeft = sf.Vector2f.new(0.0, 0.0)
    self._itemTopLeft = sf.Vector2f.new(0.0, _SHOP_TAB_HEIGHT)
    self._detailTopLeft = sf.Vector2f.new(0.0, _SHOP_TAB_HEIGHT + _SHOP_ITEM_HEIGHT)
    self._buyItemIDs = {}
    self._canSell = true
    self._mode = self.SHOP_MODE_BUY
    self._closed = true
    self._tabWindow:setVisible(false)
    self._tabWindow:setActive(false)
    self._itemWindow:setVisible(false)
    self._itemWindow:setActive(false)
    self._detailWindow:setVisible(false)
    self._detailWindow:setActive(false)
    self._transition:hideImmediate()
end

function WindowShop:getTabWindow()
    return self._tabWindow
end

function WindowShop:getItemWindow()
    return self._itemWindow
end

function WindowShop:getDetailWindow()
    return self._detailWindow
end

function WindowShop:setPlayer(player)
    self._player = player
end

function WindowShop:getVisible()
    return self._transition:isBlocking()
end

function WindowShop:isClosed()
    return self._closed
end

function WindowShop:open(buyItemIDs, canSell)
    self._buyItemIDs = WindowShop.NormalizeBuyItems(buyItemIDs)
    self._canSell = bool(canSell)
    self._mode = self.SHOP_MODE_BUY
    self._closed = false
    self._tabWindow:getTabView():setSelectedIndex(0)
    self:_refreshItems()
    self._itemWindow:resetSelection()
    self:_refreshDetail()
    if self._canSell then
        self:setPosition(self._topLeft)
        self._tabWindow:setPosition(self._tabTopLeft)
        self._itemWindow:setPosition(self._itemTopLeft)
        self._detailWindow:setPosition(self._detailTopLeft)
        self._tabWindow:setVisible(true)
        self._tabWindow:setActive(true)
    else
        local bounds = UiLayout.GetCenteredRect(_SHOP_WIDTH, _SHOP_ITEM_HEIGHT + _SHOP_DETAIL_HEIGHT)
        self:setPosition(sf.Vector2f.new(bounds.position.x, bounds.position.y))
        self._itemWindow:setPosition(sf.Vector2f.new(0.0, 0.0))
        self._detailWindow:setPosition(sf.Vector2f.new(0.0, _SHOP_ITEM_HEIGHT))
        self._tabWindow:setVisible(false)
        self._tabWindow:setActive(false)
    end
    self._itemWindow:setVisible(true)
    self._itemWindow:setActive(false)
    self._detailWindow:setVisible(true)
    self._detailWindow:setActive(false)
    self._transition:show("FadeIn", function ()
        self:setActive(true)
        if self._canSell then
            self._tabWindow:setActive(true)
        end
        self._itemWindow:setActive(true)
        self._itemWindow:requestKeyboardFocusAtCursor()
    end)
end

function WindowShop:close(onHidden)
    self._tabWindow:setActive(false)
    self._itemWindow:setActive(false)
    self._detailWindow:setActive(false)
    self:setActive(false)
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

function WindowShop:closeByCancel()
    ManagerFunctions.playSE(GameSystem.GetCancelSE())
    self:_closeAndNotify()
end

function WindowShop:handleTabNavigationInput()
    if not self._tabWindow:getVisible() then
        return false
    end
    return self._tabWindow:handleNavigationInput()
end

---@param index integer
function WindowShop:onTabSelected(index)
    self:setMode(index == 1 and self.SHOP_MODE_SELL or self.SHOP_MODE_BUY)
end

function WindowShop:setMode(mode)
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

function WindowShop:notifyItemIndexMaybeChanged()
    self:_refreshDetail()
end

function WindowShop:refreshLocale()
    self._tabWindow:refresh()
    self._detailWindow:refresh()
end

function WindowShop:confirmItem()
    local itemID = self._itemWindow:getCurrentItemID()
    if itemID == nil then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
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
function WindowShop.NormalizeBuyItems(buyItemIDs)
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

function WindowShop:_refreshItems()
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

function WindowShop:_refreshDetail()
    local itemID = self._itemWindow:getCurrentItemID()
    if itemID == nil then
        self._detailWindow:setItem(nil, nil)
        return
    end
    local price = self._mode == self.SHOP_MODE_BUY and WindowShop.GetItemPrice(itemID)
        or WindowShop.GetSellPrice(itemID)
    self._detailWindow:setItem(Data.GetGeneralItemData(itemID), price)
end

---@return table
function WindowShop:_getSellableItems()
    local itemData = Data.GetAllGeneralItemData()
    local playerItems = self._player._items or {}
    local result = {}
    for _, itemID in ipairs(table.orderedStringKeys(itemData)) do
        if (playerItems[itemID] or 0) > 0 and WindowShop.GetSellPrice(itemID) > 0 then
            result[#result + 1] = itemID
        end
    end
    return result
end

---@param itemID string
---@return integer
function WindowShop.GetItemPrice(itemID)
    local itemInfo = Data.GetGeneralItemData(itemID)
    return itemInfo.price
end

---@param itemID string
---@return integer
function WindowShop.GetSellPrice(itemID)
    return math.floor(WindowShop.GetItemPrice(itemID) / 2)
end

---@param itemID string
---@return boolean
function WindowShop:_canBuy(itemID)
    return self._player.attributes.GOLD >= WindowShop.GetItemPrice(itemID)
end

---@param itemID string
function WindowShop:_buyItem(itemID)
    local price = WindowShop.GetItemPrice(itemID)
    if not self._itemWindow:isCurrentAvailable() or self._player.attributes.GOLD < price then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        self:_refreshItems()
        return
    end
    local abilitySystem = self._player:getAbilitySystemComponent()
    abilitySystem:setNumericAttributeBase("GOLD", abilitySystem:getNumericAttributeBase("GOLD") - price)
    self._player:addItem(itemID, 1)
    ManagerFunctions.playSE(GameSystem.GetShopSE())
    self:_refreshItems()
end

---@param itemID string
function WindowShop:_sellItem(itemID)
    local price = WindowShop.GetSellPrice(itemID)
    if price <= 0 or not self._player:removeItem(itemID, 1) then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        self:_refreshItems()
        return
    end
    local abilitySystem = self._player:getAbilitySystemComponent()
    abilitySystem:setNumericAttributeBase("GOLD", abilitySystem:getNumericAttributeBase("GOLD") + price)
    ManagerFunctions.playSE(GameSystem.GetShopSE())
    self:_refreshItems()
end

function WindowShop:_closeAndNotify()
    self:close(function ()
        if self._onCloseCallback ~= nil then
            self._onCloseCallback()
        end
    end)
end

function WindowShop:dispose()
    self._transition:hideImmediate()
    self._tabWindow:setVisible(false)
    self._itemWindow:setVisible(false)
    self._detailWindow:setVisible(false)
    self._tabWindow:dispose()
    self._itemWindow:dispose()
    self._detailWindow:dispose()
    self._ui:dispose()
    self._player = nil
    self._onCloseCallback = nil
end

return class(WindowShop, Canvas)
