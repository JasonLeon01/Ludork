---@meta

---@class Source.Windows.WindowCommand.Controller: Internal.UIBase.UiController
---@field ui    Internal.UI.Parts.Title.CommandWindow
---@field host  Source.Windows.WindowCommand
---@field _rows Internal.UIBase.CommandRow.Controller[]
local Controller = {}

---@brief Bind the title's authored command rows to their confirmation callbacks.
---@param commands Internal.UIBase.CommandRow.Controller.Model[]
function Controller:init(commands) end

function Controller:refresh() end

function Controller:refreshRows() end
