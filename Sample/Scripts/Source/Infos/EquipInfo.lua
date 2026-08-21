local Engine = require("Engine")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local EquipInfo = {}

EquipInfo._infoType = GeneralDataKey.Equip

---@diagnostic disable-next-line: unused
function EquipInfo:onEquip()
end

---@diagnostic disable-next-line: unused
function EquipInfo:onUnequip()
end

return class(EquipInfo, InfoBase)
