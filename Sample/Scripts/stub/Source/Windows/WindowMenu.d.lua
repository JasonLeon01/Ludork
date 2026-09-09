---@meta

---@class Source.Windows.WindowMenuWindows
---@field item     Source.Windows.WindowItem
---@field equip    Source.Windows.WindowEquip
---@field saveLoad Source.Windows.WindowSaveLoad
---@field config   Source.Windows.ConfigWindow

---@brief In-game menu window that manages commands, open/close triggers, and sub-windows.
---
--- Owns the full menu lifecycle: detects the open trigger, defines built-in commands
--- (Items, Equipment, Save, Config, Return to Title), delegates to WindowItem, and
--- re-enables player movement on close.
---@class Source.Windows.WindowMenu.Controller: Source.UIBase.UiController
---@field host              Source.Windows.WindowMenu
---@field _player           Source.Player.Player
---@field ui                Source.UI.WindowMenu
---@field _menuControls     Engine.Canvas[]
---@field _moveRestoreGuard fun(): boolean
---@field _windowItem       Source.Windows.WindowItem
---@field _windowEquip      Source.Windows.WindowEquip
---@field _windowSaveLoad   Source.Windows.WindowSaveLoad
---@field _configWindow     Source.Windows.ConfigWindow
---@field _commands         Source.UIBase.UiCollection<Source.UIBase.CommandRow.Controller>
local Controller = {}

---@brief Construct the menu window and wire up sub-window callbacks.
---
--- - @param player The player actor; movement is disabled while the menu is open.
--- - @param windows Named item, equipment, and non-load-only save/load windows.
---@param player  Source.Player.Player
---@param windows Source.Windows.WindowMenuWindows
function Controller:init(player, windows) end

---@brief Rebind the player whose movement is controlled by the menu.
---@param player Source.Player.Player
function Controller:setPlayer(player) end

---@brief Set a predicate that decides whether close restores player movement.
---
--- - @param guard Callable returning True when movement may be restored.
---@param guard function
function Controller:setMoveRestoreGuard(guard) end

function Controller:refreshRows() end

---@brief Handle cancel key to close the menu.
---
--- - @param kwargs Event data.
---@brief Handle right-click cancel to close the menu.
---@param kwargs Engine.UiInputEventArguments
---@return boolean
function Controller:onMouseButtonDown(kwargs) end

function Controller:onReturn() end

---@param deltaTime number
function Controller:onTick(deltaTime) end

---@brief Move menu cursor or jump to the currently opened submenu.
---
--- - @param direction Navigation direction.
---
--- - @return True if the direction was handled.
---@param direction string
---@return boolean
function Controller:onDirectionalKey(direction) end

---@brief Open the menu window at its first command and disable player movement.
function Controller:open() end

---@brief Close the menu window and restore player movement.
---@param onHidden function | nil
function Controller:close(onHidden) end

---@brief Return True when the menu or its sub-windows are blocking map input.
---@return boolean
function Controller:isBlocking() end

function Controller:openInventory() end

function Controller:openEquipment() end

function Controller:openSaveLoad() end

function Controller:openConfig() end

function Controller:exitGame() end

function Controller:onSaveLoadClose() end

---@brief Reactivate the command list and return focus after the Config window closes.
function Controller:onConfigClose() end

---@return Source.Player.Player
function Controller:getPlayer() end

function Controller:refreshRows() end

---@param owner Source.Windows.WindowMenu
---@return Source.UIBase.CommandRow.Controller.Model[]
function Controller.CreateCommands(owner) end

function Controller:bind() end

---@return boolean
---@param kwargs Engine.UiInputEventArguments
function Controller:handleMouseButtonDown(kwargs) end

function Controller:tick() end

---@return boolean
---@param direction string
function Controller:handleDirectionalKey(direction) end

function Controller:handleCancel() end

function Controller:onMenuExit() end

function Controller:_closeByCancel() end

---@return Source.Windows.Base.WindowSelectable | nil
function Controller:_getCurrentSubMenuFocusTarget() end

---@param position sf.Vector2f
---@return boolean
function Controller:_isPointerInsideMenuGroup(position) end

---@param exceptName string | nil
---@return boolean
function Controller:_closeSubMenus(exceptName) end

function Controller:_syncReturnButtonSuppression() end

---@return boolean
function Controller:_returnEquipSelectToSlot() end

---@param commands Source.UIBase.CommandRow.Controller.Model[]
function Controller:attach(commands) end
