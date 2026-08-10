---@meta Source.UI.Parts.ConfigWindow.ConfigSliderRow
---@class Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI: Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase
local ConfigSliderRowUI = {}

---@return Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
function ConfigSliderRowUI.new(...) end

--- @brief Construct a configuration slider row.
---
--- - @param labelText       Localized label shown on the left
--- - @param rowWidth        Total row width in logical UI units
--- - @param sliderWidth     Slider line width
--- - @param value           Initial integer value
--- - @param onValueChanged  Callback invoked when the slider value changes
---@param labelText      string
---@param rowWidth       integer
---@param sliderWidth    integer
---@param value          integer | nil
---@param onValueChanged function | nil
function ConfigSliderRowUI:init(labelText, rowWidth, sliderWidth, value, onValueChanged) end

function ConfigSliderRowUI:bind() end

function ConfigSliderRowUI:refresh() end

---@return Engine.Canvas
function ConfigSliderRowUI:prepare() end

--- @brief Get the row Slider coordinator.
---@return Engine.Slider
function ConfigSliderRowUI:getSlider() end

---@param active boolean
function ConfigSliderRowUI:setActive(active) end

--- @brief Adjust the nested slider value.
---@param delta integer
function ConfigSliderRowUI:adjust(delta) end

function ConfigSliderRowUI:dispose() end

return ConfigSliderRowUI
