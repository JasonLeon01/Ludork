---@meta Source.Windows.WindowShopItem

--- @brief Two-column shop item list.
---@class Source.Windows.WindowShopItem: Source.Windows.Base.WindowSelectable
---@field _owner Source.Windows.WindowShop
---@field _itemIDs string[]
---@field _cellAvailable boolean[]
---@field _ui Source.UI.Parts.WindowShop.WindowShopItem.WindowShopItemUI
---@field _listView Engine.ListView
local WindowShopItem = {}

---@param rect sf.IntRect
---@param owner Source.Windows.WindowShop
---@return Source.Windows.WindowShopItem
function WindowShopItem.new(rect, owner) end

--- @brief Construct the shop item list.
---
--- - @param rect The item window rectangle.
--- - @param owner The shop coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowShop
function WindowShopItem:init(rect, owner) end

--- @brief Rebuild the displayed shop item list.
---
--- - @param itemIDs Ordered item IDs to display.
--- - @param availableMap Item availability by ID.
--- - @param valueMap Right-side numeric value by ID.
---@param itemIDs      table
---@param availableMap table
---@param valueMap     table
function WindowShopItem:refreshItems(itemIDs, availableMap, valueMap) end

---@return string | nil
function WindowShopItem:getCurrentItemID() end

---@return boolean
function WindowShopItem:isCurrentAvailable() end

---@param kwargs table
function WindowShopItem:onKeyDown(kwargs) end

---@param kwargs table
---@return boolean
function WindowShopItem:onMouseButtonDown(kwargs) end

return WindowShopItem
