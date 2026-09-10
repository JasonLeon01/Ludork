---@meta Source.Windows.WindowCommand

---@class Source.Windows.WindowCommand.ViewParts
---@field windowFrame Engine.Window
---@field content     Engine.Canvas
---@field scrollBox   Engine.ScrollBox
---@field listView    Engine.ListView

---@brief A simple selectable command list window.
---
--- Provides a vertical list of command items with callbacks
--- triggered on confirmation.
---@class Source.Windows.WindowCommand: Source.Windows.Base.WindowSelectable
---@field _rows Source.UIBase.UiCollection<Source.UIBase.CommandRow.Controller>
---@field new   fun(rect: sf.IntRect, commands?: Source.UIBase.CommandRow.Controller.Model[], rectWidth?: integer, rectHeight?: integer, windowSkin?: sf.Image, repeated?: boolean, columns?: integer, externalView?: Source.Windows.WindowCommand.ViewParts): Source.Windows.WindowCommand
local WindowCommand = {}

---@brief Construct a command window with the given commands.
---
--- - @param rect The window rectangle.
--- - @param commands Ordered command models using exactly one of `localeKey` or `text`.
--- - @param rectWidth Optional fixed width for the selection rectangle.
--- - @param rectHeight Height of each command item.
--- - @param windowSkin Optional window skin image.
--- - @param repeated Whether the window skin is repeated.
---@param rect         sf.IntRect
---@param commands     Source.UIBase.CommandRow.Controller.Model[] | nil
---@param rectWidth    integer | nil
---@param rectHeight   integer | nil
---@param windowSkin   sf.Image | nil
---@param repeated     boolean | nil
---@param columns      integer | nil
---@param externalView Source.Windows.WindowCommand.ViewParts | nil
function WindowCommand:init(rect, commands, rectWidth, rectHeight, windowSkin, repeated, columns, externalView) end

function WindowCommand:refreshRows() end

function WindowCommand:dispose() end

return WindowCommand
