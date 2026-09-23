---@meta Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller
---@class Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller: Source.Windows.ConfigWindow.ConfigRow
---@field ui Internal.UI.Parts.ConfigWindow.ConfigCheckBoxRow
local ConfigCheckBoxRowController = {}

---@param ui               Internal.UI.Parts.ConfigWindow.ConfigCheckBoxRow
---@param labelText        string
---@param checked          boolean | nil
---@param onCheckedChanged fun(checked: boolean) | nil
---@return Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller
function ConfigCheckBoxRowController.new(ui, labelText, checked, onCheckedChanged) end

---@brief Bind configuration checkbox behaviour to an existing generated row View.
---@param ui               Internal.UI.Parts.ConfigWindow.ConfigCheckBoxRow
---@param labelText        string
---@param checked          boolean | nil
---@param onCheckedChanged fun(checked: boolean) | nil
function ConfigCheckBoxRowController:init(ui, labelText, checked, onCheckedChanged) end

function ConfigCheckBoxRowController:bind() end

function ConfigCheckBoxRowController:refresh() end

---@param active boolean
function ConfigCheckBoxRowController:setActive(active) end

return ConfigCheckBoxRowController
