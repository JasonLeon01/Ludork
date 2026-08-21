local Engine = require("Engine")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local ItemInfo = {}

ItemInfo._infoType = GeneralDataKey.Item

function ItemInfo:onUse()
    local _ = self
end

function ItemInfo:onDrop()
    local _ = self
end

return class(ItemInfo, InfoBase)
