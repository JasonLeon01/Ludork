---@meta Source.Windows.ConfigWindow.ConfigSliderRow.Controller
---@class Source.Windows.ConfigWindow.ConfigSliderRow.Controller: Source.Windows.ConfigWindow.ConfigRow
---@field ui Source.UI.Parts.ConfigWindow.ConfigSliderRow
local ConfigSliderRowController = {}

---@param ui             Source.UI.Parts.ConfigWindow.ConfigSliderRow
---@param labelText      string
---@param value          integer | nil
---@param onValueChanged fun(value: integer) | nil
---@return Source.Windows.ConfigWindow.ConfigSliderRow.Controller
function ConfigSliderRowController.new(ui, labelText, value, onValueChanged) end

---@brief Bind configuration volume behaviour to an existing generated row View.
---@param ui             Source.UI.Parts.ConfigWindow.ConfigSliderRow
---@param labelText      string
---@param value          integer | nil
---@param onValueChanged fun(value: integer) | nil
function ConfigSliderRowController:init(ui, labelText, value, onValueChanged) end

function ConfigSliderRowController:bind() end

function ConfigSliderRowController:refresh() end

---@param active boolean
function ConfigSliderRowController:setActive(active) end

---@brief Adjust the nested slider value.
---@param delta integer
function ConfigSliderRowController:adjust(delta) end

return ConfigSliderRowController
