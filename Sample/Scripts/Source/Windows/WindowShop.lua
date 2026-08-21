local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local UiLayout = require("Source.UI.UiLayout")
local WindowShopCommand = require("Source.Windows.WindowShopCommand")
local WindowShopItem = require("Source.Windows.WindowShopItem")

local ManagerFunctions = GlobalFunctions.Manager
local GlobalSystem = GlobalCore.System

local _SHOP_COMMAND_HEIGHT = 64
local _SHOP_ITEM_SIZE = 352
local _SHOP_WIDTH = 352

local _ITEM_ORDER = {
    "KEY_Y", "KEY_B", "KEY_R", "EnemyBook", "Teleport", "BreakWall", "ClearWall", "BreakLava", "BreakIce",
    "PoisonedEase", "PoisonedRelease", "WeakEase", "WeakRelease"
}

---@class Source.Windows.WindowShop
local WindowShop = {}

WindowShop.SHOP_MODE_BUY = "buy"
WindowShop.SHOP_MODE_SELL = "sell"

function WindowShop.GetDefaultRects()
    local totalHeight = _SHOP_COMMAND_HEIGHT + _SHOP_ITEM_SIZE
    local bounds = UiLayout.GetCenteredRect(_SHOP_WIDTH, totalHeight)
    return Engine.ToIntRect(bounds.position.x, bounds.position.y, _SHOP_WIDTH, _SHOP_COMMAND_HEIGHT),
        Engine.ToIntRect(
            bounds.position.x, bounds.position.y + _SHOP_COMMAND_HEIGHT, _SHOP_ITEM_SIZE, _SHOP_ITEM_SIZE
        )
end

function WindowShop:init(player, commandRect, itemRect, onClose)
    if commandRect == nil or itemRect == nil then
        local defaultCommandRect, defaultItemRect = WindowShop.GetDefaultRects()
        commandRect = commandRect or defaultCommandRect
        itemRect = itemRect or defaultItemRect
    end
    self._player = player
    self._onCloseCallback = onClose
    self._commandWindow = WindowShopCommand.new(commandRect, self)
    self._itemWindow = WindowShopItem.new(itemRect, self)
    self._itemTopLeft = sf.Vector2f.new(itemRect.position.x, itemRect.position.y)
    self._buyItemIDs = {}
    self._canSell = true
    self._mode = self.SHOP_MODE_BUY
    self._closed = true
    self:close()
end

function WindowShop:getCommandWindow()
    return self._commandWindow
end

function WindowShop:getItemWindow()
    return self._itemWindow
end

function WindowShop:setPlayer(player)
    self._player = player
end

function WindowShop:getVisible()
    return self._itemWindow:getVisible()
end

function WindowShop:isClosed()
    return self._closed
end

function WindowShop:open(buyItemIDs, canSell)
    self._buyItemIDs = WindowShop.NormalizeBuyItems(buyItemIDs)
    self._canSell = bool(canSell)
    self._mode = self.SHOP_MODE_BUY
    self._closed = false
    self._commandWindow.index = 0
    self._commandWindow._lastIndex = 0
    self:_refreshItems()
    if self._canSell then
        self._commandWindow:setVisible(true)
        self._commandWindow:setActive(true)
        self._itemWindow:setPosition(self._itemTopLeft)
        self._itemWindow:setActive(false)
        self._commandWindow:requestKeyboardFocus()
    else
        local gameSize = GlobalSystem.getGameSize()
        local topLeft = sf.Vector2f.new((gameSize.x - _SHOP_ITEM_SIZE) / 2, (gameSize.y - _SHOP_ITEM_SIZE) / 2)
        self._itemWindow:setPosition(topLeft)
        self._commandWindow:setVisible(false)
        self._commandWindow:setActive(false)
        self._itemWindow:setActive(true)
        self._itemWindow:requestKeyboardFocus()
    end
    self._itemWindow:setVisible(true)
end

function WindowShop:close()
    self._commandWindow:setVisible(false)
    self._commandWindow:setActive(false)
    self._itemWindow:setVisible(false)
    self._itemWindow:setActive(false)
    self._closed = true
end

function WindowShop:closeByCancel()
    ManagerFunctions.playSE(GameSystem.getCancelSE())
    self:_closeAndNotify()
end

function WindowShop:setMode(mode)
    if mode ~= self.SHOP_MODE_BUY and mode ~= self.SHOP_MODE_SELL then
        return
    end
    if not self._canSell and mode == self.SHOP_MODE_SELL then
        return
    end
    self._mode = mode
    self:_refreshItems()
end

function WindowShop:confirmCommand()
    self:setMode(
        self._commandWindow.index == 1 and self.SHOP_MODE_SELL or self.SHOP_MODE_BUY
    )
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    self._commandWindow:setActive(false)
    self._itemWindow:setActive(true)
    self._itemWindow:requestKeyboardFocusAtCursor()
end

function WindowShop:cancelItemSelection()
    ManagerFunctions.playSE(GameSystem.getCancelSE())
    if not self._canSell then
        self:_closeAndNotify()
        return
    end
    self._itemWindow:setActive(false)
    self._commandWindow:setActive(true)
    self._commandWindow:requestKeyboardFocus()
end

function WindowShop:confirmItem()
    local itemID = self._itemWindow:getCurrentItemID()
    if itemID == nil then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
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
    local itemData = Data.getAllGeneralItemData()
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
    if self._mode == self.SHOP_MODE_BUY then
        itemIDs = self._buyItemIDs
        for _, itemID in ipairs(itemIDs) do
            availableMap[itemID] = self:_canBuy(itemID)
            valueMap[itemID] = WindowShop.GetItemPrice(itemID)
        end
    else
        itemIDs = self:_getSellableItems()
        for _, itemID in ipairs(itemIDs) do
            availableMap[itemID] = true
            valueMap[itemID] = self._player:getItemCount(itemID)
        end
    end
    self._itemWindow:refreshItems(itemIDs, availableMap, valueMap)
end

---@return table
function WindowShop:_getSellableItems()
    local itemData = Data.getAllGeneralItemData()
    local playerItems = self._player._items or {}
    local result = {}
    local included = {}
    for _, itemID in ipairs(_ITEM_ORDER) do
        if itemData[itemID] ~= nil and (playerItems[itemID] or 0) > 0 and WindowShop.GetItemPrice(itemID) > 0 then
            result[#result + 1] = itemID
            included[itemID] = true
        end
    end
    local extras = {}
    for itemID in pairs(itemData) do
        if not included[itemID] and (playerItems[itemID] or 0) > 0 and WindowShop.GetItemPrice(itemID) > 0 then
            extras[#extras + 1] = itemID
        end
    end
    table.sort(extras)
    for _, itemID in ipairs(extras) do
        result[#result + 1] = itemID
    end
    return result
end

---@param itemID string
---@return integer
function WindowShop.GetItemPrice(itemID)
    local itemInfo = Data.getGeneralItemData(itemID)
    return itemInfo.price
end

---@param itemID string
---@return boolean
function WindowShop:_canBuy(itemID)
    return self._player.infoComp.GOLD >= WindowShop.GetItemPrice(itemID)
end

---@param itemID string
function WindowShop:_buyItem(itemID)
    local price = WindowShop.GetItemPrice(itemID)
    if not self._itemWindow:isCurrentAvailable() or self._player.infoComp.GOLD < price then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        self:_refreshItems()
        return
    end
    self._player.infoComp.GOLD = self._player.infoComp.GOLD - price
    self._player:addItem(itemID, 1)
    ManagerFunctions.playSE(GameSystem.getShopSE())
    self:_refreshItems()
end

---@param itemID string
function WindowShop:_sellItem(itemID)
    local price = WindowShop.GetItemPrice(itemID)
    if price <= 0 or not self._player:removeItem(itemID, 1) then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        self:_refreshItems()
        return
    end
    self._player.infoComp.GOLD = self._player.infoComp.GOLD + price
    ManagerFunctions.playSE(GameSystem.getShopSE())
    self:_refreshItems()
end

function WindowShop:_closeAndNotify()
    self:close()
    if self._onCloseCallback ~= nil then
        self._onCloseCallback()
    end
end

return class(WindowShop)
