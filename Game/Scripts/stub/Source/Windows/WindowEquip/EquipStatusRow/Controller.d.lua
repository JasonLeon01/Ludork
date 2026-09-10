---@meta Source.Windows.WindowEquip.EquipStatusRow.Controller

---@class Source.Windows.WindowEquip.EquipStatusRow.ControllerModel
---@field label string
---@field delta integer

---@class Source.Windows.WindowEquip.EquipStatusRow.Controller: Source.UIBase.UiController
---@field ui    Source.UI.Parts.WindowEquip.EquipStatusRow
---@field model Source.Windows.WindowEquip.EquipStatusRow.ControllerModel
---@field new   fun(model: Source.Windows.WindowEquip.EquipStatusRow.ControllerModel): Source.Windows.WindowEquip.EquipStatusRow.Controller
local EquipStatusRowController = {}

function EquipStatusRowController:refresh() end

return EquipStatusRowController
