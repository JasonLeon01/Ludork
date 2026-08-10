---@meta Source.UI.Helpers.CommandRow

---@class Source.UI.Helpers.CommandRow
---@field model { text: string, callback: function | nil }
---@field root Engine.Canvas
---@field _bound boolean
---@field new fun(model: { text: string, callback: function | nil }): Source.UI.Helpers.CommandRow
local CommandRowController = {}

function CommandRowController:init(model) end

function CommandRowController:bind() end

function CommandRowController:refresh() end

function CommandRowController:prepare(logicalSize) end

return CommandRowController
