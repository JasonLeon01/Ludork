local Engine = require("Engine")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local EquipInfo = {}

EquipInfo._infoType = GeneralDataKey.Equip

function EquipInfo:onEquip()
    local _ = self
end

function EquipInfo:onUnequip()
    local _ = self
end

return class(EquipInfo, InfoBase)
