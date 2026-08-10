local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

local GlobalSystem = GlobalCore.System

local UiLayout = {}

function UiLayout.GetCenteredRect(width, height)
    local gameSize = GlobalSystem.getGameSize()
    local x = math.floor((gameSize.x - width) / 2)
    local y = math.floor((gameSize.y - height) / 2)
    return Engine.ToIntRect(x, y, width, height)
end

return UiLayout
