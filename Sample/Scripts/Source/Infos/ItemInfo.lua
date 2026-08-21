local Engine = require("Engine")
---@type { GeneralDataKey: Source.Configs.GeneralEnum.GeneralDataKey }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local InfoBase = Engine.InfoBase
local GeneralDataKey = GeneralEnum.GeneralDataKey

local ItemInfo = {}

ItemInfo._infoType = GeneralDataKey.Item

---@diagnostic disable-next-line: unused
function ItemInfo:onUse()
end

---@diagnostic disable-next-line: unused
function ItemInfo:onDrop()
end

return class(ItemInfo, InfoBase)
