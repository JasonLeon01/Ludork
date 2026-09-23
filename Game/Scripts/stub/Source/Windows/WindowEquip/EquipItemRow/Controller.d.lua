---@meta Source.Windows.WindowEquip.EquipItemRow.Controller

---@class Source.Windows.WindowEquip.EquipItemRow.Controller.Model
---@field iconTexture sf.Texture | nil
---@field count       integer

---@class Source.Windows.WindowEquip.EquipItemRow.Controller: Internal.UIBase.UiController
---@field ui    Internal.UI.Parts.WindowEquip.EquipItemRow
---@field model Source.Windows.WindowEquip.EquipItemRow.Controller.Model
---@field new   fun(model: Source.Windows.WindowEquip.EquipItemRow.Controller.Model): Source.Windows.WindowEquip.EquipItemRow.Controller
local EquipItemRowController = {}

function EquipItemRowController:refresh() end

return EquipItemRowController
