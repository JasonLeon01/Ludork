---@meta

---
--- Shows player inventory items in a grid with icons and counts.
--- Uses WindowSelectable for keyboard/mouse navigation.
---@class Source.Windows.WindowItem.Controller: Internal.UIBase.UiController
---@field host               Source.Windows.WindowItem
---@field _onCloseCallback   function | nil
---@field _onUseCallback     function | nil
---@field _player            Source.MapActors.Player.Player
---@field ui                 Internal.UI.WindowItem
---@field _itemList          { [1]: string, [2]: integer } []
---@field _lastDescIndex     integer | nil
---@field _rows              Internal.UIBase.UiCollection<Source.Windows.WindowItem.ItemRow.Controller>
---@field _transitionProfile string
local Controller = {}

---@brief Construct the item window.
---
--- - @param player The player instance with inventory.
--- - @param onClose Optional callback invoked when the window is closed.
---@param player  Source.MapActors.Player.Player
---@param onClose function | nil
function Controller:init(player, onClose) end

---@brief Rebind the player whose inventory is displayed.
---@param player Source.MapActors.Player.Player
function Controller:setPlayer(player) end

---@brief Update item window and render item cells.
---
--- - @param deltaTime Elapsed time in seconds.
---@param deltaTime number
function Controller:onTick(deltaTime) end

---@brief Open the item window, refreshing inventory and selecting its first item.
---
--- Defaults to centered scale fade. Menu transitions require the owning menu dock position.
---@param transitionProfile string | nil
---@param dockPosition      sf.Vector2f | nil
function Controller:open(transitionProfile, dockPosition) end

---@brief Refresh the currently selected item's localised name and description.
function Controller:refreshLocale() end

---@brief Close the item window.
---@param onHidden function | nil
function Controller:close(onHidden) end

---@brief Close the item window through its cancel path.
function Controller:onReturn() end

---@return Source.MapActors.Player.Player
function Controller:getPlayer() end

---@param callback function | nil
function Controller:setOnCloseCallback(callback) end

---@param callback function | nil
function Controller:setOnUseCallback(callback) end

function Controller:onItemUsed() end

function Controller:notifyClosed() end

function Controller:refresh() end

function Controller:refreshItems() end

function Controller:tick() end

---@param text string
---@return string
function Controller:wrapDescription(text) end

function Controller:updateDescription() end

function Controller:useSelectedItem() end

function Controller:closeByCancel() end

function Controller:ready() end
