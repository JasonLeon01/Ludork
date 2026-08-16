local Engine = require("Engine")
local WindowShopItemUI = require("Source.UI.Parts.WindowShop.WindowShopItem")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
local SHOP_ITEM_ROW_HEIGHT = 32

---@class Source.Windows.WindowShopItem
local WindowShopItem = {}

WindowShopItem.uiClass = WindowShopItemUI

function WindowShopItem:init(rect, owner)
    super(WindowShopItem, self).init(rect, nil, nil, SHOP_ITEM_ROW_HEIGHT)
    self:setHasReturnBtn(true)
    self._owner = owner
    self._itemIDs = {}
    self._cellAvailable = {}
    self._ui = self.uiClass.new(self, rect.size)
    self._ui:attach()
    self._listView = self._ui:getListView()
end

function WindowShopItem:refreshItems(itemIDs, availableMap, valueMap)
    local previousIndex = self.index
    local previousItemID = self:getCurrentItemID()
    self._itemIDs = copy(itemIDs)
    self._ui:refreshItems(itemIDs, availableMap, valueMap)
    if not bool(itemIDs) then
        self.index = nil
    else
        local restoredIndex = nil
        if previousItemID ~= nil then
            for luaIndex, itemID in ipairs(itemIDs) do
                if itemID == previousItemID then
                    restoredIndex = luaIndex - 1
                    break
                end
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
    self._ui:detachControl(self._rect)
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

function WindowShopItem:onKeyDown(kwargs)
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    super(WindowShopItem, self).onKeyDown(kwargs)
end

function WindowShopItem:onMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Right then
        self:onReturn()
        return true
    end
    return false
end

function WindowShopItem:onReturn()
    self._owner:cancelItemSelection()
end

return class(WindowShopItem, WindowSelectable)
