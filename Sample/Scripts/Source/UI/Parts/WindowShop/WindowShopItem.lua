local Data = require("Source.Data")
local IconTexture = require("Source.UI.IconTexture")
local Ui = require("Source.UI.Ui")
local WindowShopCellUI = require("Source.UI.Parts.WindowShop.WindowShopCell")

local _SHOP_ITEM_ROW_HEIGHT = 32

local WindowShopItemUI = {}

function WindowShopItemUI:init(model, size, instance)
    self._size = size
    self._cellControllers = {}
    super(WindowShopItemUI, self).init(model, instance)
end

function WindowShopItemUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("ItemScrollBox")
    self._listView = self:requireControl("ItemList")
end

function WindowShopItemUI:prepare()
    return super(WindowShopItemUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowShopItemUI:attach(nested)
    if nested == true then
        self:attachNestedWindowView(self.model)
    else
        self:attachWindowView(self.model)
    end
end

function WindowShopItemUI:getWindowFrame()
    return self._windowFrame
end

function WindowShopItemUI:getContent()
    return self._content
end

function WindowShopItemUI:getListView()
    return self._listView
end

function WindowShopItemUI:getScrollBox()
    return self._scrollBox
end

function WindowShopItemUI:refreshItems(itemIDs, availableMap, valueMap, showValues)
    self._listView:clearChildren()
    self._cellControllers = {}
    self.model._cellAvailable = {}
    local cellWidth = self.model:_getRectWidth()
    local itemData = Data.GetAllGeneralItemData()
    for _, itemID in ipairs(itemIDs) do
        local member = itemData[itemID] or {}
        local available = availableMap[itemID]
        if available == nil then
            available = true
        end
        self.model._cellAvailable[#self.model._cellAvailable + 1] = available
        local cellController = WindowShopCellUI.new({
            iconTexture = IconTexture.Load(member.icon or ""),
            value = valueMap[itemID] or 0,
            showValue = showValues,
            available = available,
            callback = function (_obj, _kwargs)
                self.model._owner:confirmItem()
            end
        })
        local cell = cellController:prepare(sf.Vector2u.new(cellWidth, _SHOP_ITEM_ROW_HEIGHT))
        self._cellControllers[#self._cellControllers + 1] = cellController
        self._listView:addChild(cell)
    end
end

return Ui.Define("Parts/WindowShop/WindowShopItem", WindowShopItemUI)
