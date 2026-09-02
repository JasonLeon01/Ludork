---@meta Source.UI.WindowEquip

---@class Source.UI.WindowEquip: Source.UI.UiController
---@field new fun(model: Source.Windows.WindowEquip): Source.UI.WindowEquip
local WindowEquipUI = {}

function WindowEquipUI:attach() end

---@return Engine.AssetInstance
function WindowEquipUI:getSlotAsset() end

---@return Engine.AssetInstance
function WindowEquipUI:getSelectAsset() end

---@return Engine.AssetInstance
function WindowEquipUI:getStatusPaneAsset() end

return WindowEquipUI
