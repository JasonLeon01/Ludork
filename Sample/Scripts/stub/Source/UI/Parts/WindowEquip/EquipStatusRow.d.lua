---@meta Source.UI.Parts.WindowEquip.EquipStatusRow

---@class Source.UI.Parts.WindowEquip.EquipStatusRowModel
---@field label string
---@field delta integer

---@class Source.UI.Parts.WindowEquip.EquipStatusRow: Source.UI.UiController
---@field model Source.UI.Parts.WindowEquip.EquipStatusRowModel
---@field new fun(model: Source.UI.Parts.WindowEquip.EquipStatusRowModel): Source.UI.Parts.WindowEquip.EquipStatusRow
local EquipStatusRowUI = {}

function EquipStatusRowUI:refresh() end

return EquipStatusRowUI
