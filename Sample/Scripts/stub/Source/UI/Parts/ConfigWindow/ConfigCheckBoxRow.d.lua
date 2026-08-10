---@meta Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow
---@class Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI: Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase
local ConfigCheckBoxRowUI = {}

---@return Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
function ConfigCheckBoxRowUI.new(...) end

--- @brief Construct a configuration checkbox row.
---
--- - @param labelText         Localized label shown on the left
--- - @param rowWidth          Total row width in logical UI units
--- - @param checkBoxSize      Checkbox width and height
--- - @param windowSkin        Windowskin shared with the parent window
--- - @param checked           Initial checked state
--- - @param onCheckedChanged  Callback invoked when the checked state changes
---@param labelText        string
---@param rowWidth         integer
---@param checkBoxSize     integer
---@param windowSkin       sf.Image
---@param checked          boolean | nil
---@param onCheckedChanged function | nil
function ConfigCheckBoxRowUI:init(labelText, rowWidth, checkBoxSize, windowSkin, checked, onCheckedChanged) end

function ConfigCheckBoxRowUI:bind() end

function ConfigCheckBoxRowUI:refresh() end

--- @brief Get the row CheckBox coordinator.
---
--- - @return  Nested CheckBox instance
---@return Engine.CheckBox
function ConfigCheckBoxRowUI:getCheckBox() end

---@param active boolean
function ConfigCheckBoxRowUI:setActive(active) end

function ConfigCheckBoxRowUI:dispose() end

return ConfigCheckBoxRowUI
