---@meta Source.Windows.WindowMenu.Controller

---@class Source.Windows.WindowMenu.Controller: Source.Windows.WindowCommand.Controller
---@field model             Source.Windows.WindowMenu
---@field _menuControls     Engine.Canvas[]
---@field _moveRestoreGuard fun(): boolean
---@field new               fun(model: Source.Windows.WindowMenu, size: sf.Vector2u, rowHeight: integer, columns: integer, windows: Source.Windows.WindowMenuWindows): Source.Windows.WindowMenu.Controller
---@field _windowItem       Source.Windows.WindowItem
---@field _windowEquip      Source.Windows.WindowEquip
---@field _windowSaveLoad   Source.Windows.WindowSaveLoad
---@field _configWindow     Source.Windows.ConfigWindow
local WindowMenuController = {}

---@param owner Source.Windows.WindowMenu
---@return Source.UI.Parts.Shared.CommandRowModel[]
function WindowMenuController.CreateCommands(owner) end

function WindowMenuController:bind() end

---@param guard fun(): boolean
function WindowMenuController:setMoveRestoreGuard(guard) end

---@return boolean
---@param kwargs Engine.UiInputEventArguments
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

function WindowMenuController:handleCancel() end

function WindowMenuController:onSaveLoadClose() end

function WindowMenuController:onConfigClose() end

function WindowMenuController:onMenuExit() end

function WindowMenuController:_closeByCancel() end

function WindowMenuController:openInventory() end

function WindowMenuController:openEquipment() end

function WindowMenuController:openSaveLoad() end

function WindowMenuController:openConfig() end

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

---@param model     Source.Windows.WindowMenu
---@param size      sf.Vector2u
---@param rowHeight integer
---@param columns   integer
---@param windows   Source.Windows.WindowMenuWindows
function WindowMenuController:init(model, size, rowHeight, columns, windows) end

return WindowMenuController
