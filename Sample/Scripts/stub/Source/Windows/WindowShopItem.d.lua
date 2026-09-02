---@meta Source.Windows.WindowShopItem

---@brief Two-column shop item list.
---@class Source.Windows.WindowShopItem: Source.Windows.Base.WindowSelectable
---@field new              fun(rect: sf.IntRect, owner: Source.Windows.WindowShop, instance?: Engine.AssetInstance): Source.Windows.WindowShopItem
---@field _owner           Source.Windows.WindowShop
---@field _itemIDs         string[]
---@field _cellAvailable   boolean[]
---@field _lastDetailIndex integer | nil
---@field _ui              Source.UI.Parts.WindowShop.WindowShopItem.WindowShopItemUI
---@field _listView        Engine.ListView
local WindowShopItem = {}

---@param rect  sf.IntRect
---@param owner Source.Windows.WindowShop
---@return Source.Windows.WindowShopItem
function WindowShopItem.new(rect, owner) end

---@brief Construct the shop item list.
---
--- - @param rect The item window rectangle.
--- - @param owner The shop coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowShop
function WindowShopItem:init(rect, owner, instance) end

---@brief Rebuild the displayed shop item list.
---
--- - @param itemIDs Ordered item IDs to display.
--- - @param availableMap Item availability by ID.
--- - @param valueMap Right-side numeric value by ID.
--- - @param showValues Whether right-side values are visible.
---@param itemIDs      table
---@param availableMap table
---@param valueMap     table
---@param showValues   boolean
function WindowShopItem:refreshItems(itemIDs, availableMap, valueMap, showValues) end

---@param deltaTime number
function WindowShopItem:onTick(deltaTime) end

---@param kwargs table
function WindowShopItem:onKeyDown(kwargs) end

function WindowShopItem:resetSelection() end

---@return string | nil
function WindowShopItem:getCurrentItemID() end

---@return boolean
function WindowShopItem:isCurrentAvailable() end

function WindowShopItem:onReturn() end

function WindowShopItem:dispose() end

return WindowShopItem
