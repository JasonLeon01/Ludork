local Engine = require("Engine")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local ItemInfo = {}

ItemInfo._infoType = GeneralDataKey.Item

function ItemInfo:onUse()
end

function ItemInfo:onGet()
end

return class(ItemInfo, InfoBase)
