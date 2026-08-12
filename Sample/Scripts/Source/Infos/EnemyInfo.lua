local Engine = require("Engine")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local EnemyInfo = {}

EnemyInfo._infoType = GeneralDataKey.Enemy

function EnemyInfo:onDefeat()
    local _ = self
end

return class(EnemyInfo, InfoBase)
