---@meta

---@brief Two-column shop item list.
---@class Source.Windows.WindowShopItem.Controller: Internal.UIBase.UiController
---@field host             Source.Windows.WindowShopItem
---@field _owner           Source.Windows.WindowShop
---@field _itemIDs         string[]
---@field _lastDetailIndex integer | nil
---@field _listView        Engine.ListView
---@field ui               Internal.UI.Parts.WindowShop.WindowShopItem
---@field _cellAvailable   boolean[]
---@field _cells           Internal.UIBase.UiCollection<Source.Windows.WindowShop.WindowShopCell.Controller>
local Controller = {}

---@brief Construct the shop item list.
---
--- - @param owner The shop coordinator.
---@param owner Source.Windows.WindowShop
function Controller:init(owner) end

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
function Controller:refreshItems(itemIDs, availableMap, valueMap, showValues) end

---@param deltaTime number
function Controller:onTick(deltaTime) end

---@param kwargs Engine.UiInputEventArguments
function Controller:onKeyDown(kwargs) end

function Controller:resetSelection() end

---@return string | nil
function Controller:getCurrentItemID() end

---@return boolean
function Controller:isCurrentAvailable() end

function Controller:onReturn() end

function Controller:dispose() end

function Controller:confirmItem() end
