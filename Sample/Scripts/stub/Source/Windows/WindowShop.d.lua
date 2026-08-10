---@meta Source.Windows.WindowShop

--- @brief Integrated shop UI with optional command bar and item list.
---@class Source.Windows.WindowShop
---@field SHOP_MODE_BUY "buy"
---@field SHOP_MODE_SELL "sell"
---@field _player Source.Player.Player
---@field _onCloseCallback function | nil
---@field _commandWindow Source.Windows.WindowShopCommand
---@field _itemWindow Source.Windows.WindowShopItem
---@field _itemTopLeft sf.Vector2f
---@field _buyItemIDs string[]
---@field _canSell boolean
---@field _mode string
---@field _closed boolean
local WindowShop = {}

---@return sf.IntRect, sf.IntRect
function WindowShop.GetDefaultRects() end

---@param buyItemIDs table
---@return table
function WindowShop.NormalizeBuyItems(buyItemIDs) end

---@param itemID string
---@return integer
function WindowShop.GetItemPrice(itemID) end

--- @brief Construct the shop coordinator.
---
--- - @param player The player whose inventory and GOLD are modified.
--- - @param commandRect Rectangle for the buy/sell command bar.
--- - @param itemRect Rectangle for the item list window.
--- - @param onClose Callback invoked after the shop closes.
---@param player      Source.Player.Player
---@param commandRect sf.IntRect | nil
---@param itemRect    sf.IntRect | nil
---@param onClose     function | nil
function WindowShop:init(player, commandRect, itemRect, onClose) end

--- @brief Get the shop command window.
---
--- - @return The command window.
---@return Source.Windows.WindowShopCommand
function WindowShop:getCommandWindow() end

--- @brief Get the shop item window.
---
--- - @return The item window.
---@return Source.Windows.WindowShopItem
function WindowShop:getItemWindow() end

--- @brief Rebind the player instance used by the shop.
---
--- - @param player The new player instance.
---@param player Source.Player.Player
function WindowShop:setPlayer(player) end

--- @brief Return whether the shop UI is visible.
---
--- - @return True when the item window is visible.
---@return boolean
function WindowShop:getVisible() end

--- @brief Return whether the latest shop session has closed.
---
--- - @return True after close.
---@return boolean
function WindowShop:isClosed() end

--- @brief Open the shop with the provided buy list and sell flag.
---
--- - @param buyItemIDs Item IDs available for purchase.
--- - @param canSell Whether the sell command is available.
---@param buyItemIDs table
---@param canSell    boolean
function WindowShop:open(buyItemIDs, canSell) end

--- @brief Close and deactivate both shop windows.
function WindowShop:close() end

--- @brief Close the whole shop via cancel input.
function WindowShop:closeByCancel() end

--- @brief Switch buy/sell mode and refresh the item list.
---
--- - @param mode The shop mode.
---@param mode string
function WindowShop:setMode(mode) end

--- @brief Confirm buy/sell mode and move focus to the item list.
function WindowShop:confirmCommand() end

--- @brief Cancel item selection, returning to command bar or closing shop.
---@return boolean
function WindowShop:cancelItemSelection() end

--- @brief Confirm the selected item and perform buy/sell.
function WindowShop:confirmItem() end

return WindowShop
