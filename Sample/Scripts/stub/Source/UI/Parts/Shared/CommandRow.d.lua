---@meta Source.UI.Parts.Shared.CommandRow

---@class Source.UI.Parts.Shared.LocalizedCommandRowModel
---@field localeKey string
---@field text      nil
---@field callback  function | nil

---@class Source.UI.Parts.Shared.TextCommandRowModel
---@field text      string
---@field localeKey nil
---@field callback  function | nil

---@alias Source.UI.Parts.Shared.CommandRowModel Source.UI.Parts.Shared.LocalizedCommandRowModel | Source.UI.Parts.Shared.TextCommandRowModel

---@class Source.UI.Parts.Shared.CommandRow: Source.UI.UiController
---@field model Source.UI.Parts.Shared.CommandRowModel
---@field new   fun(model: Source.UI.Parts.Shared.CommandRowModel): Source.UI.Parts.Shared.CommandRow
local CommandRowUI = {}

---@param model Source.UI.Parts.Shared.CommandRowModel
function CommandRowUI:init(model) end

function CommandRowUI:bind() end

function CommandRowUI:refresh() end

return CommandRowUI
