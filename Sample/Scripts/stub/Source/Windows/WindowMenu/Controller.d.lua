---@meta Source.Windows.WindowMenu.Controller

---@class Source.Windows.WindowMenu.Controller: Source.Windows.WindowCommand.Controller
---@field model             Source.Windows.WindowMenu
---@field _menuControls     Engine.Canvas[]
---@field _moveRestoreGuard fun(): boolean
---@field new               fun(model: Source.Windows.WindowMenu, size: sf.Vector2u, rowHeight: integer, columns: integer): Source.Windows.WindowMenu.Controller
local WindowMenuController = {}

---@param owner Source.Windows.WindowMenu
---@return Source.UI.Parts.Shared.CommandRowModel[]
function WindowMenuController.CreateCommands(owner) end

function WindowMenuController:bind() end

---@param guard fun(): boolean
function WindowMenuController:setMoveRestoreGuard(guard) end

---@return boolean
---@param kwargs table
function WindowMenuController:handleMouseButtonDown(kwargs) end

function WindowMenuController:tick() end

---@return boolean
---@param direction string
function WindowMenuController:handleDirectionalKey(direction) end

function WindowMenuController:open() end

---@param onHidden function | nil
function WindowMenuController:close(onHidden) end

---@return boolean
function WindowMenuController:isBlocking() end

function WindowMenuController:_handleCancel() end

---@return Engine.Canvas[]
function WindowMenuController:getMenuControls() end

function WindowMenuController:onSaveLoadClose() end

function WindowMenuController:onConfigClose() end

function WindowMenuController:onMenuExit() end

function WindowMenuController:_closeByCancel() end

function WindowMenuController:_onMenuItem() end

function WindowMenuController:_onMenuEquip() end

function WindowMenuController:_onMenuSave() end

function WindowMenuController:_onMenuConfig() end

---@return Source.Windows.Base.WindowSelectable | nil
function WindowMenuController:_getCurrentSubMenuFocusTarget() end

---@param position sf.Vector2f
---@return boolean
function WindowMenuController:_isPointerInsideMenuGroup(position) end

---@param exceptName string | nil
---@return boolean
function WindowMenuController:_closeSubMenus(exceptName) end

function WindowMenuController:_syncReturnButtonSuppression() end

---@return boolean
function WindowMenuController:_returnEquipSelectToSlot() end

return WindowMenuController
