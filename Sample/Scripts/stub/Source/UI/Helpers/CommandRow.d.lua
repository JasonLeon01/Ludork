---@meta Source.UI.Helpers.CommandRow

---@class Source.UI.Helpers.LocalizedCommandRowModel
---@field localeKey string
---@field text nil
---@field callback function | nil

---@class Source.UI.Helpers.TextCommandRowModel
---@field text string
---@field localeKey nil
---@field callback function | nil

---@alias Source.UI.Helpers.CommandRowModel Source.UI.Helpers.LocalizedCommandRowModel | Source.UI.Helpers.TextCommandRowModel

---@class Source.UI.Helpers.CommandRow
---@field model Source.UI.Helpers.CommandRowModel
---@field root Engine.Canvas
---@field _bound boolean
---@field new fun(model: Source.UI.Helpers.CommandRowModel): Source.UI.Helpers.CommandRow
local CommandRowController = {}

---@param model Source.UI.Helpers.CommandRowModel
function CommandRowController:init(model) end

function CommandRowController:bind() end

function CommandRowController:refresh() end

---@param logicalSize sf.Vector2u
---@return Engine.Canvas
function CommandRowController:prepare(logicalSize) end

return CommandRowController
