---@meta Source.Infos.EquipInfo
--- @brief Equip data + logic layer.
---
--- Defines equip-related blueprint events (onEquip, onUnequip).
--- Independent of Actor; can be used standalone in inventory/shop UI.
---
local EquipInfo = {}

---@return any
function EquipInfo.new(...) end

--- @brief Triggered when the equip is equipped.
function EquipInfo:onEquip() end

--- @brief Triggered when the equip is unequipped.
function EquipInfo:onUnequip() end

return EquipInfo
