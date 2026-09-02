local Engine = require("Engine")
local WindowShopItemUI = require("Source.UI.Parts.WindowShop.WindowShopItem")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local SHOP_ITEM_ROW_HEIGHT = 32

---@class Source.Windows.WindowShopItem
local WindowShopItem = {}

WindowShopItem.uiClass = WindowShopItemUI

function WindowShopItem:init(rect, owner, instance)
    super(WindowShopItem, self).init(rect, nil, rect.size.x - 64, SHOP_ITEM_ROW_HEIGHT, nil, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._owner = owner
    self._itemIDs = {}
    self._cellAvailable = {}
    self._lastDetailIndex = nil
    self._ui = self.uiClass.new(self, rect.size, instance)
    self._ui:attach(instance ~= nil)
    self:setScrollBox(self._ui:getScrollBox())
    self:setListView(self._ui:getListView())
end

function WindowShopItem:refreshItems(itemIDs, availableMap, valueMap, showValues)
    local previousIndex = self.index ~= nil and self.index or nil
    local previousItemID = self:getCurrentItemID()
    self._itemIDs = copy(itemIDs)
    self._ui:refreshItems(itemIDs, availableMap, valueMap, showValues)
    if not bool(itemIDs) then
        self.index = nil
    else
        local restoredIndex = nil
        if previousItemID ~= nil then
            local luaIndex = table.index(itemIDs, previousItemID)
            if luaIndex ~= nil then
                restoredIndex = luaIndex - 1
            end
        end
        if restoredIndex ~= nil then
            self.index = restoredIndex
        elseif previousIndex ~= nil then
            self.index = Engine.ToInteger(math.min(previousIndex, #itemIDs - 1))
        else
            self.index = 0
        end
    end
    self._lastDetailIndex = self.index
    self._ui:detachControl(self._rect)
end

function WindowShopItem:onTick(deltaTime)
    super(WindowShopItem, self).onTick(deltaTime)
    if self.index ~= self._lastDetailIndex then
        self._lastDetailIndex = self.index
        self._owner:notifyItemIndexMaybeChanged()
    end
end

function WindowShopItem:onKeyDown(kwargs)
    if self._owner:handleTabNavigationInput() then
        return
    end
    super(WindowShopItem, self).onKeyDown(kwargs)
end

function WindowShopItem:resetSelection()
    super(WindowShopItem, self).resetSelection()
    self._lastDetailIndex = self.index
end

function WindowShopItem:getCurrentItemID()
    if self.index == nil or self.index >= #self._itemIDs then
        return nil
    end
    return self._itemIDs[self.index + 1]
end

function WindowShopItem:isCurrentAvailable()
    if self.index == nil or self.index >= #self._cellAvailable then
        return false
    end
    return self._cellAvailable[self.index + 1]
end

function WindowShopItem:onReturn()
    self._owner:closeByCancel()
end

function WindowShopItem:dispose()
    self._ui:dispose()
    self._ui = nil
    self:setListView(nil)
    self._owner = nil
end

return class(WindowShopItem, WindowSelectable)
