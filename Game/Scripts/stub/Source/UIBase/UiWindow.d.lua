---@meta Source.UIBase.UiWindow

---@class Source.UIBase.Ui.WindowOptions
---@field position?         sf.Vector2f
---@field centered?         boolean
---@field screen?           boolean
---@field hidden?           boolean
---@field returnButton?     boolean
---@field focusable?        boolean
---@field frame?            string
---@field content?          string
---@field list?             string
---@field scroll?           string
---@field transitionTarget? string

---@class Source.UIBase.UiWindow
---@field host        Engine.Canvas
---@field ui          Source.UIBase.UiView
---@field options     Source.UIBase.Ui.WindowOptions
---@field nested      boolean
---@field transition  Source.UIBase.WindowTransition
---@field _position   sf.Vector2f | nil
---@field _manager    GlobalCore.UIManager | nil
---@field _selectable boolean | nil
local UiWindow = {}

---@param host       Engine.Canvas
---@param ui         Source.UIBase.UiView
---@param nativeBase table
---@param options    Source.UIBase.Ui.WindowOptions
---@param nested     boolean
---@return Source.UIBase.UiWindow
function UiWindow.new(host, ui, nativeBase, options, nested) end

---@param host       Engine.Canvas
---@param ui         Source.UIBase.UiView
---@param nativeBase table
---@param options    Source.UIBase.Ui.WindowOptions
---@param nested     boolean
function UiWindow:init(host, ui, nativeBase, options, nested) end

function UiWindow:attach() end
function UiWindow:syncLayout() end

---@param position sf.Vector2f
function UiWindow:setPosition(position) end

---@param manager GlobalCore.UIManager
---@return Engine.Canvas
function UiWindow:mount(manager) end

function UiWindow:unmount() end
function UiWindow:dispose() end

return UiWindow
