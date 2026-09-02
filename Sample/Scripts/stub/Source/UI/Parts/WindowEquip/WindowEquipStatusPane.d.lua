---@meta Source.UI.Parts.WindowEquip.WindowEquipStatusPane

---@class Source.UI.Parts.WindowEquip.WindowEquipStatusPane: Source.UI.UiController
---@field new fun(model: Source.Windows.WindowEquipStatus, instance: Engine.AssetInstance): Source.UI.Parts.WindowEquip.WindowEquipStatusPane
local WindowEquipStatusPaneUI = {}

function WindowEquipStatusPaneUI:attach() end

---@return Engine.Window
function WindowEquipStatusPaneUI:getWindowFrame() end

---@return Engine.Canvas
function WindowEquipStatusPaneUI:getContent() end

---@return Engine.AssetInstance
function WindowEquipStatusPaneUI:getStatusAsset() end

return WindowEquipStatusPaneUI
