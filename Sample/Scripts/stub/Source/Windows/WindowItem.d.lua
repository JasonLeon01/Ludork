---@meta Source.Windows.WindowItem
---
--- Shows player inventory items in a grid with icons and counts.
--- Uses WindowSelectable for keyboard/mouse navigation.
---@class Source.Windows.WindowItem: Source.Windows.Base.WindowSelectable
---@field _onCloseCallback function | nil
---@field _onUseCallback   function | nil
---@field new              fun(rect: sf.IntRect, player: Source.Player.Player, onClose?: function): Source.Windows.WindowItem
---@field _player          Source.Player.Player
local WindowItem = {}

---@brief Construct the item window.
---
--- - @param rect The window rectangle.
--- - @param player The player instance with inventory.
--- - @param onClose Optional callback invoked when the window is closed.
---@param rect    sf.IntRect
---@param player  Source.Player.Player
---@param onClose function | nil
function WindowItem:init(rect, player, onClose) end

---@brief Rebind the player whose inventory is displayed.
---@param player Source.Player.Player
function WindowItem:setPlayer(player) end

---@brief Update item window and render item cells.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function WindowItem:onTick(deltaTime) end

---@brief Open the item window, refreshing inventory first.
function WindowItem:open() end

---@brief Refresh the currently selected item's localised name and description.
function WindowItem:refreshLocale() end

---@brief Close the item window.
function WindowItem:close() end

---@brief Close the item window through its cancel path.
function WindowItem:onReturn() end

return WindowItem
