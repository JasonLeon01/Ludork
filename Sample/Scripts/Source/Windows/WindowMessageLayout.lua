local Engine = require("Engine")
local Data = require("Source.Data")

local TextLayout = Engine.TextLayout

local WindowMessageLayout = {}

function WindowMessageLayout.NormaliseText(text)
    return (text:gsub("\\n", "\n"))
end

function WindowMessageLayout.GetFadeCurve(cache, key)
    local cached = cache[key]
    if cached ~= nil then
        return cached
    end
    local curve = Data.GetCurve(key)
    cache[key] = curve
    return curve
end

function WindowMessageLayout.GetTextLineHeight(bounds)
    return math.max(1, math.ceil(bounds.position.y + bounds.size.y))
end

function WindowMessageLayout.WrapMessage(text, maxWidth, textConfigKey)
    return TextLayout.wrapRichText(text, maxWidth, textConfigKey)
end

function WindowMessageLayout.ResizeCanvas(target, width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    target:resize(logicalSize)
    target:setView(target:getDefaultView())
end

return WindowMessageLayout
