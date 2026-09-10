---@meta Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@class Source.Windows.ConfigWindow.ConfigSettingRow.Controller: Source.Windows.ConfigWindow.ConfigRow
---@field ui Source.UI.Parts.ConfigWindow.ConfigSettingRow
local ConfigSettingRowController = {}

---@param ui            Source.UI.Parts.ConfigWindow.ConfigSettingRow
---@param labelText     string
---@param items         string[]
---@param selectedIndex integer | nil
---@return Source.Windows.ConfigWindow.ConfigSettingRow.Controller
function ConfigSettingRowController.new(ui, labelText, items, selectedIndex) end

---@brief Bind configuration options to an existing generated row View.
---@param ui            Source.UI.Parts.ConfigWindow.ConfigSettingRow
---@param labelText     string
---@param items         string[]
---@param selectedIndex integer | nil
function ConfigSettingRowController:init(ui, labelText, items, selectedIndex) end

function ConfigSettingRowController:bind() end

function ConfigSettingRowController:open() end

function ConfigSettingRowController:refresh() end

---@param items string[]
function ConfigSettingRowController:setItems(items) end

---@param active boolean
function ConfigSettingRowController:setActive(active) end

return ConfigSettingRowController
