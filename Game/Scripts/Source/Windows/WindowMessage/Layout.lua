local Engine = require("Engine")

local TextLayout = Engine.TextLayout

local WindowMessageLayout = {}

function WindowMessageLayout.NormaliseText(text)
    return (text:gsub("\\n", "\n"))
end

function WindowMessageLayout.GetTextLineHeight(bounds)
    return math.max(1, math.ceil(bounds.position.y + bounds.size.y))
end

function WindowMessageLayout.WrapMessage(text, maxWidth, control)
    return TextLayout.wrapRichText(text, maxWidth, control)
end

return WindowMessageLayout
