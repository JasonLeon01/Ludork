---@meta Source.Windows.WindowMenu

---@brief In-game menu window that manages commands, open/close triggers, and sub-windows.
---
--- Owns the full menu lifecycle: detects the open trigger, defines built-in commands
--- (Items, Equipment, Save, Config, Return to Title), delegates to WindowItem, and
--- re-enables player movement on close.
---@class Source.Windows.WindowMenu: Source.Windows.Base.WindowSelectable
---@field controllerClass Source.Windows.WindowMenu.Controller
---@field _player         Source.Player.Player
---@field _menuController Source.Windows.WindowMenu.Controller
---@field new             fun(player: Source.Player.Player, windows: Source.Windows.WindowMenuWindows): Source.Windows.WindowMenu
local WindowMenu = {}

---@class Source.Windows.WindowMenuWindows
---@field item     Source.Windows.WindowItem
---@field equip    Source.Windows.WindowEquip
---@field saveLoad Source.Windows.WindowSaveLoad
---@field config   Source.Windows.ConfigWindow

---@brief Construct the menu window and wire up sub-window callbacks.
---
--- - @param player The player actor; movement is disabled while the menu is open.
--- - @param windows Named item, equipment, and non-load-only save/load windows.
---@param player  Source.Player.Player
---@param windows Source.Windows.WindowMenuWindows
function WindowMenu:init(player, windows) end

---@brief Rebind the player whose movement is controlled by the menu.
---@param player Source.Player.Player
function WindowMenu:setPlayer(player) end

---@brief Set a predicate that decides whether close restores player movement.
---
--- - @param guard Callable returning True when movement may be restored.
---@param guard function
function WindowMenu:setMoveRestoreGuard(guard) end

function WindowMenu:refreshRows() end

---@brief Handle cancel key to close the menu.
---
--- - @param kwargs Event data.
---@brief Handle right-click cancel to close the menu.
---@param kwargs Engine.UiInputEventArguments
---@return boolean
function WindowMenu:onMouseButtonDown(kwargs) end

function WindowMenu:onReturn() end

---@param deltaTime number
function WindowMenu:onTick(deltaTime) end

---@brief Move menu cursor or jump to the currently opened submenu.
---
--- - @param direction Navigation direction.
---
--- - @return True if the direction was handled.
---@param direction string
---@return boolean
function WindowMenu:onDirectionalKey(direction) end

---@brief Open the menu window at its first command and disable player movement.
function WindowMenu:open() end

---@brief Close the menu window and restore player movement.
---@param onHidden function | nil
function WindowMenu:close(onHidden) end

---@brief Return True when the menu or its sub-windows are blocking map input.
---@return boolean
function WindowMenu:isBlocking() end

function WindowMenu:openInventory() end

function WindowMenu:openEquipment() end

function WindowMenu:openSaveLoad() end

function WindowMenu:openConfig() end

function WindowMenu:exitGame() end

function WindowMenu:onSaveLoadClose() end

---@brief Reactivate the command list and return focus after the Config window closes.
function WindowMenu:onConfigClose() end

---@return Source.Player.Player
function WindowMenu:getPlayer() end

return WindowMenu
