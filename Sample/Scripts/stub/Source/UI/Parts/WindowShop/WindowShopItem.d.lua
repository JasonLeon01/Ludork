---@meta Source.UI.Parts.WindowShop.WindowShopItem

---@class Source.UI.Parts.WindowShop.WindowShopItem.WindowShopItemUI : Source.UI.UiController
local WindowShopItemUI = {}

---@return Source.UI.Parts.WindowShop.WindowShopItem.WindowShopItemUI
function WindowShopItemUI.new(...) end

---@param iconPath string
---@return sf.Texture | nil
function WindowShopItemUI.loadItemIcon(iconPath) end

---@param model Source.Windows.WindowShopItem
---@param size  sf.Vector2i
function WindowShopItemUI:init(model, size) end

function WindowShopItemUI:bind() end

---@return Engine.Canvas
function WindowShopItemUI:prepare() end

function WindowShopItemUI:attach() end

---@return Engine.Window
function WindowShopItemUI:getWindowFrame() end

---@return Engine.Canvas
function WindowShopItemUI:getContent() end

---@return Engine.ListView
function WindowShopItemUI:getListView() end

---@param itemIDs      table
---@param availableMap table
---@param valueMap     table
function WindowShopItemUI:refreshItems(itemIDs, availableMap, valueMap) end

return WindowShopItemUI
