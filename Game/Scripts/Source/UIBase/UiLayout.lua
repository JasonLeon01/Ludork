local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

local Display = GlobalCore.Display
local UiLayout = {}

function UiLayout.GetCenteredRect(width, height)
    local gameSize = Display.getGameSize()
    local x = math.floor((gameSize.x - width) / 2)
    local y = math.floor((gameSize.y - height) / 2)
    return Engine.ToIntRect(x, y, width, height)
end

function UiLayout.GetMenuDockPosition()
    return sf.Vector2f.new(192, 0)
end

function UiLayout.GetCenteredPosition(width, height)
    local bounds = UiLayout.GetCenteredRect(width, height)
    return sf.Vector2f.new(bounds.position.x, bounds.position.y)
end

function UiLayout.ResizeCanvas(target, width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    target:resize(logicalSize)
    target:setView(target:getDefaultView())
end

return UiLayout
