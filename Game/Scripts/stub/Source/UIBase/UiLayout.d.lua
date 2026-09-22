---@meta Source.UIBase.UiLayout

---@class Source.UIBase.UiLayout.Module
local UiLayout = {}

---@param width  integer
---@param height integer
---@return sf.IntRect
function UiLayout.GetCenteredRect(width, height) end

---@return sf.Vector2f
---@param menu Engine.ControlBase
function UiLayout.GetMenuDockPosition(menu) end

---@param width  integer
---@param height integer
---@return sf.Vector2f
function UiLayout.GetCenteredPosition(width, height) end

--- Resize a Canvas and restore its default view to match the new dimensions.
---@param target Engine.Canvas
---@param width  integer
---@param height integer
function UiLayout.ResizeCanvas(target, width, height) end

return UiLayout
