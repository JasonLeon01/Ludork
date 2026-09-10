---@meta Source.Windows.WindowEquip.EquipSlotRow.Controller

---@class Source.Windows.WindowEquip.EquipSlotRow.Controller.Model
---@field iconTexture sf.Texture | nil
---@field label       string

---@class Source.Windows.WindowEquip.EquipSlotRow.Controller: Source.UIBase.UiController
---@field ui    Source.UI.Parts.WindowEquip.EquipSlotRow
---@field model Source.Windows.WindowEquip.EquipSlotRow.Controller.Model
---@field new   fun(model: Source.Windows.WindowEquip.EquipSlotRow.Controller.Model): Source.Windows.WindowEquip.EquipSlotRow.Controller
local EquipSlotRowController = {}

function EquipSlotRowController:refresh() end

return EquipSlotRowController
