---@meta Source.Infos.EquipInfo
--- @brief Equip data + logic layer.
---
--- Defines equip-related blueprint events (onEquip, onUnequip).
--- Independent of Actor; can be used standalone in inventory/shop UI.
---
---@class Source.Infos.EquipInfo: Engine.InfoBase
---@field new fun(): Source.Infos.EquipInfo
local EquipInfo = {}

--- @brief Triggered when the equip is equipped.
function EquipInfo:onEquip() end

--- @brief Triggered when the equip is unequipped.
function EquipInfo:onUnequip() end

return EquipInfo
