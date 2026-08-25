---@meta Source.Windows.WindowMenu

---@class Source.Windows.WindowMenu.Controller: Source.Windows.WindowCommand.Controller
---@field model             Source.Windows.WindowMenu
---@field _menuControls     Engine.Canvas[]
---@field _moveRestoreGuard fun(): boolean
local WindowMenuController = {}

---@param owner Source.Windows.WindowMenu
---@return Source.UI.Helpers.CommandRowModel[]
function WindowMenuController.CreateCommands(owner) end

function WindowMenuController:bind() end

function WindowMenuController:setMoveRestoreGuard(guard) end

---@return boolean
function WindowMenuController:handleMouseButtonDown(kwargs) end

function WindowMenuController:tick() end

---@return boolean
function WindowMenuController:handleDirectionalKey(direction) end

function WindowMenuController:open() end

function WindowMenuController:close() end

---@return boolean
function WindowMenuController:isBlocking() end

function WindowMenuController:_handleCancel() end

---@return Engine.Canvas[]
function WindowMenuController:getMenuControls() end

function WindowMenuController:onSaveLoadClose() end

function WindowMenuController:onConfigClose() end

---@brief In-game menu window that manages commands, open/close triggers, and sub-windows.
---
--- Owns the full menu lifecycle: detects the open trigger, defines built-in commands
--- (Items, Equipment, Save, Config, Return to Title), delegates to WindowItem, and
--- re-enables player movement on close.
---@class Source.Windows.WindowMenu: Source.Windows.WindowCommand
---@field controllerClass    Class.ClassType<Source.Windows.WindowMenu.Controller>
---@field _player            Source.Player.Player
---@field _windowItem        Source.Windows.WindowItem
---@field _windowEquipSlot   Source.Windows.WindowEquipSlot
---@field _windowEquipSelect Source.Windows.WindowEquipSelect
---@field _windowEquipStatus Source.Windows.WindowEquipStatus
---@field _windowSaveLoad    Source.Windows.WindowSaveLoad
---@field _configWindow      Source.Windows.ConfigWindow
---@field _menuController    Source.Windows.WindowMenu.Controller
---@field _menuControls      Engine.Canvas[]
---@field new                fun(player: Source.Player.Player, windows: Source.Windows.WindowMenuWindows): Source.Windows.WindowMenu
local WindowMenu = {}

---@class Source.Windows.WindowMenuWindows
---@field item        Source.Windows.WindowItem
---@field equipSlot   Source.Windows.WindowEquipSlot
---@field equipSelect Source.Windows.WindowEquipSelect
---@field equipStatus Source.Windows.WindowEquipStatus
---@field saveLoad    Source.Windows.WindowSaveLoad
---@field config      Source.Windows.ConfigWindow

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

---@brief Handle cancel key to close the menu.
---
--- - @param kwargs Event data.
---@brief Handle right-click cancel to close the menu.
---@param kwargs table
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
function WindowMenu:close() end

---@brief Return True when the menu or its sub-windows are blocking map input.
---@return boolean
function WindowMenu:isBlocking() end

function WindowMenu:_onMenuItem() end

function WindowMenu:_onMenuEquip() end

function WindowMenu:_onMenuSave() end

function WindowMenu:_onMenuConfig() end

---@return Source.Windows.Base.WindowSelectable | nil
function WindowMenu:_getCurrentSubMenuFocusTarget() end

---@param position sf.Vector2f
---@return boolean
function WindowMenu:_isPointerInsideMenuGroup(position) end

---@param exceptName string | nil
---@return boolean
function WindowMenu:_closeSubMenus(exceptName) end

---@return boolean
function WindowMenu:_returnEquipSelectToSlot() end

---@return boolean
function WindowMenu:_returnSaveSlotToCommand() end

function WindowMenu:onSaveLoadClose() end

---@brief Reactivate the command list and return focus after the Config window closes.
function WindowMenu:onConfigClose() end

return WindowMenu
