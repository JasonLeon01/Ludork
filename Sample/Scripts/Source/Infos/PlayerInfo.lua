local Engine = require("Engine")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local PlayerInfo = {}

PlayerInfo._infoType = GeneralDataKey.Player

return class(PlayerInfo, InfoBase)
