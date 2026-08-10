---@meta Source.UI.Parts.ConfigWindow.ConfigSettingRow
---@class Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI: Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase
local ConfigSettingRowUI = {}

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigSettingRowUI.new(...) end

--- @brief Construct a configuration setting row.
---
--- - @param labelText      Localized label shown on the left
--- - @param items          DropBox option labels
--- - @param rowWidth       Total row width in logical UI units
--- - @param dropboxWidth   DropBox field width
--- - @param windowSkin     Windowskin shared with the parent window
--- - @param selectedIndex  Initial DropBox selection index
---@param labelText     string
---@param items         table
---@param rowWidth      integer
---@param dropboxWidth  integer
---@param windowSkin    sf.Image
---@param selectedIndex integer | nil
function ConfigSettingRowUI:init(labelText, items, rowWidth, dropboxWidth, windowSkin, selectedIndex) end

function ConfigSettingRowUI:bind() end

function ConfigSettingRowUI:refresh() end

--- @brief Get the row DropBox coordinator.
---
--- - @return  Nested DropBox instance
---@return Engine.DropBox
function ConfigSettingRowUI:getDropBox() end

---@param items table
function ConfigSettingRowUI:setItems(items) end

---@param active boolean
function ConfigSettingRowUI:setActive(active) end

function ConfigSettingRowUI:dispose() end

return ConfigSettingRowUI
