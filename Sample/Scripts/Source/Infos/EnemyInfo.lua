local Engine = require("Engine")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local EnemyInfo = {}

EnemyInfo._infoType = GeneralDataKey.Enemy

function EnemyInfo:onDefeat()
end

return class(EnemyInfo, InfoBase)
