---@meta

---@class Source.Windows.WindowMenuWindows
---@field item     Internal.UIBase.LazyWindow<Source.Windows.WindowItem>
---@field equip    Internal.UIBase.LazyWindow<Source.Windows.WindowEquip>
---@field saveLoad Internal.UIBase.LazyWindow<Source.Windows.WindowSaveLoad>
---@field config   Internal.UIBase.LazyWindow<Source.Windows.ConfigWindow>

---@brief In-game menu window that manages commands, open/close triggers, and sub-windows.
---
--- Defines commands and sub-window navigation, then restores player movement on close.
--- The owning scene controls opening, stacking and the exit action.
---@class Source.Windows.WindowMenu.Controller: Internal.UIBase.UiController
---@field host              Source.Windows.WindowMenu
---@field _player           Source.MapActors.Player.Player
---@field ui                Internal.UI.WindowMenu
---@field _moveRestoreGuard fun(): boolean
---@field _onExit           fun()
---@field _windowItem       Internal.UIBase.LazyWindow<Source.Windows.WindowItem>
---@field _windowEquip      Internal.UIBase.LazyWindow<Source.Windows.WindowEquip>
---@field _windowSaveLoad   Internal.UIBase.LazyWindow<Source.Windows.WindowSaveLoad>
---@field _configWindow     Internal.UIBase.LazyWindow<Source.Windows.ConfigWindow>
---@field _commands         Internal.UIBase.UiCollection<Internal.UIBase.CommandRow.Controller>
local Controller = {}

---@brief Construct the menu window without creating its independent sub-windows.
---
--- - @param player The player actor; movement is disabled while the menu is open.
--- - @param windows Named lazy handles for item, equipment, non-load-only save/load, and configuration windows.
--- - @param onExit Scene-owned action invoked after the menu closes for Exit.
---@param player  Source.MapActors.Player.Player
---@param windows Source.Windows.WindowMenuWindows
---@param onExit  fun()
function Controller:init(player, windows, onExit) end

---@brief Rebind the player whose movement is controlled by the menu.
---@param player Source.MapActors.Player.Player
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

---@brief Restore the return button and command focus after an item or equipment window closes.
function Controller:onSubMenuClose() end

---@brief Reactivate the command list and return focus after the Config window closes.
function Controller:onConfigClose() end

---@return Source.MapActors.Player.Player
function Controller:getPlayer() end

function Controller:refreshRows() end

---@param owner Source.Windows.WindowMenu
---@return Internal.UIBase.CommandRow.Controller.Model[]
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

---@return Internal.UIBase.WindowSelectable | nil
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

---@param commands Internal.UIBase.CommandRow.Controller.Model[]
function Controller:attach(commands) end
