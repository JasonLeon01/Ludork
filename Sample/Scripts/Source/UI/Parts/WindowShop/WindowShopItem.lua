local Data = require("Source.Data")
local IconTexture = require("Source.UI.IconTexture")
local Ui = require("Source.UI.Ui")
local WindowShopCellUI = require("Source.UI.Parts.WindowShop.WindowShopCell")

local _SHOP_ITEM_ROW_HEIGHT = 32

local WindowShopItemUI = {}

function WindowShopItemUI:init(model, size)
    self._size = size
    self._cellControllers = {}
    super(WindowShopItemUI, self).init(model, nil)
end

function WindowShopItemUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._listView = self:requireControl("ItemList")
end

function WindowShopItemUI:prepare()
    return super(WindowShopItemUI, self).prepare(sf.Vector2u.new(self._size.x, self._size.y))
end

function WindowShopItemUI:attach()
    self:attachWindowView(self.model)
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

function WindowShopItemUI:refreshItems(itemIDs, availableMap, valueMap)
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
            iconTexture = IconTexture.LoadItem(member.icon or ""),
            value = valueMap[itemID] or 0,
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
