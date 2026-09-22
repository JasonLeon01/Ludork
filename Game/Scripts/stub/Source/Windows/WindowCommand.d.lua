---@meta

---@class Source.Windows.WindowCommand.Controller: Source.UIBase.UiController
---@field ui    Source.UI.Parts.Title.CommandWindow
---@field host  Source.Windows.WindowCommand
---@field _rows Source.UIBase.CommandRow.Controller[]
local Controller = {}

---@brief Bind the title's authored command rows to their confirmation callbacks.
---@param commands Source.UIBase.CommandRow.Controller.Model[]
function Controller:init(commands) end

function Controller:refresh() end

function Controller:refreshRows() end
