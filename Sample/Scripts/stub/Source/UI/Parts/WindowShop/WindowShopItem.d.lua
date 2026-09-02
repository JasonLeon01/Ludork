---@meta Source.UI.Parts.WindowShop.WindowShopItem

---@class Source.UI.Parts.WindowShop.WindowShopItem.WindowShopItemUI: Source.UI.UiController
local WindowShopItemUI = {}

---@return Source.UI.Parts.WindowShop.WindowShopItem.WindowShopItemUI
function WindowShopItemUI.new(...) end

---@param model Source.Windows.WindowShopItem
---@param size  sf.Vector2i
function WindowShopItemUI:init(model, size, instance) end

function WindowShopItemUI:bind() end

---@return Engine.Canvas
function WindowShopItemUI:prepare() end

function WindowShopItemUI:attach(nested) end

---@return Engine.Window
function WindowShopItemUI:getWindowFrame() end

---@return Engine.Canvas
function WindowShopItemUI:getContent() end

---@return Engine.ListView
function WindowShopItemUI:getListView() end

---@return Engine.ScrollBox
function WindowShopItemUI:getScrollBox() end

---@param itemIDs      table
---@param availableMap table
---@param valueMap     table
---@param showValues   boolean
function WindowShopItemUI:refreshItems(itemIDs, availableMap, valueMap, showValues) end

return WindowShopItemUI
