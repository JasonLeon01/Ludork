local Data = require("Source.Data")
local IconTexture = require("Source.UIBase.IconTexture")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowShop.WindowShopItem")
local WindowShopCellController = require("Source.Windows.WindowShop.WindowShopCell.Controller")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local SHOP_ITEM_ROW_HEIGHT = 32

---@class Source.Windows.WindowShopItem.Controller
local Controller = {}

Controller.windowOptions = {
    returnButton = true,
    hidden = true,
    list = "ItemList",
    scroll = "ItemScrollBox",
    itemHeight = SHOP_ITEM_ROW_HEIGHT
}

function Controller:init(owner)
    self._owner = owner
    self._itemIDs = {}
    self._lastDetailIndex = nil
    self._cellAvailable = {}
    self._cells = self:createCollection(self.ui.controls["ItemList"], WindowShopCellController)
end

function Controller:refreshItems(itemIDs, availableMap, valueMap, showValues)
    local previousIndex = self.host.index ~= nil and self.host.index or nil
    local previousItemID = self:getCurrentItemID()
    self._itemIDs = copy(itemIDs)
    self._cells:clear()
    self._cellAvailable = {}
    local cellWidth = self.host:getItemWidth()
    local itemData = Data.GetAllGeneralItemData()
    for _, itemID in ipairs(itemIDs) do
        local member = itemData[itemID] or {}
        local available = availableMap[itemID]
        if available == nil then
            available = true
        end
        self._cellAvailable[#self._cellAvailable + 1] = available
        local rowSize = sf.Vector2u.new(cellWidth, SHOP_ITEM_ROW_HEIGHT)
        ---@cast rowSize sf.Vector2u
        self._cells:add({
            iconTexture = IconTexture.Load(member.icon or ""),
            value = valueMap[itemID] or 0,
            showValue = showValues,
            available = available,
            callback = self:bindCallback(Controller.confirmItem)
        }, rowSize)
    end
    self._cells:layout()
    if not bool(itemIDs) then
        self.host.index = nil
    else
        local restoredIndex = nil
        if previousItemID ~= nil then
            local luaIndex = table.index(itemIDs, previousItemID)
            if luaIndex ~= nil then
                restoredIndex = luaIndex - 1
            end
        end
        if restoredIndex ~= nil then
            self.host.index = restoredIndex
        elseif previousIndex ~= nil then
            self.host.index = math.trunc(math.min(previousIndex, #itemIDs - 1))
        else
            self.host.index = 0
        end
    end
    self._lastDetailIndex = self.host.index
    self.host:detachSelectionRect()
end

function Controller:onTick(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
    if self.host.index ~= self._lastDetailIndex then
        self._lastDetailIndex = self.host.index
        self._owner:notifyItemIndexMaybeChanged()
    end
end

function Controller:onKeyDown(kwargs)
    if self._owner:handleTabNavigationInput() then
        return
    end
    WindowSelectable.onKeyDown(self.host, kwargs)
end

function Controller:resetSelection()
    WindowSelectable.resetSelection(self.host)
    self._lastDetailIndex = self.host.index
end

function Controller:getCurrentItemID()
    if self.host.index == nil or self.host.index >= #self._itemIDs then
        return nil
    end
    return self._itemIDs[self.host.index + 1]
end

function Controller:isCurrentAvailable()
    return self.host.index ~= nil and self.host.index >= 0 and self._cellAvailable[self.host.index + 1] == true
end

function Controller:confirmItem()
    self._owner:confirmItem()
end

function Controller:onReturn()
    self._owner:closeByCancel()
end

function Controller:dispose()
    self.host:setListView(nil)
    self._owner = nil
    super(Controller, self).dispose()
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
