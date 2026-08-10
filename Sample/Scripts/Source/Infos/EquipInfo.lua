local Engine = require("Engine")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local EquipInfo = {}

EquipInfo._infoType = GeneralDataKey.Equip

function EquipInfo:onEquip()
end

function EquipInfo:onUnequip()
end

return class(EquipInfo, InfoBase)
