---@meta Source.Windows.WindowCommand

---@class Source.Windows.WindowCommand.Controller: Source.UI.Helpers.ListView
---@field model Source.Windows.WindowCommand
---@field _size sf.Vector2u
---@field _rowHeight integer
---@field _columns integer
---@field _rowWidth integer
---@field _rowControllers Source.UI.Helpers.CommandRow[]
local WindowCommandController = {}

function WindowCommandController:init(model, size, rowHeight, columns) end

function WindowCommandController:refresh() end

function WindowCommandController:prepare() end

---@param commands table
function WindowCommandController:attach(commands) end

---@param item table
function WindowCommandController:addRow(item) end

function WindowCommandController:refreshRows() end

---@param item table
---@return Engine.ControlBase
function WindowCommandController:createRow(item) end

--- @brief A simple selectable command list window.
---
--- Provides a vertical list of command items with callbacks
--- triggered on confirmation.
---@class Source.Windows.WindowCommand: Source.Windows.Base.WindowSelectable
---@field Controller Class.ClassType<Source.Windows.WindowCommand.Controller>
---@field controllerClass Class.ClassType<Source.Windows.WindowCommand.Controller>
---@field _commandController Source.Windows.WindowCommand.Controller
---@field new fun(rect: sf.IntRect, commands?: table, rectWidth?: integer, rectHeight?: integer, windowSkin?: sf.Image, repeated?: boolean, columns?: integer): Source.Windows.WindowCommand
local WindowCommand = {}

--- @brief Construct a command window with the given commands.
---
--- - @param rect The window rectangle.
--- - @param commands Dictionary of command key to {text, callback}.
--- - @param rectWidth Optional fixed width for the selection rectangle.
--- - @param rectHeight Height of each command item.
--- - @param windowSkin Optional window skin image.
--- - @param repeated Whether the window skin is repeated.
---@param rect       sf.IntRect
---@param commands   table | nil
---@param rectWidth  integer | nil
---@param rectHeight integer | nil
---@param windowSkin sf.Image | nil
---@param repeated   boolean | nil
---@param columns    integer | nil
function WindowCommand:init(rect, commands, rectWidth, rectHeight, windowSkin, repeated, columns) end

function WindowCommand:refreshRows() end

return WindowCommand
