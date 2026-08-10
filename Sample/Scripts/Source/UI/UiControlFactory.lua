local Engine = require("Engine")

local Canvas = Engine.Canvas
local ListView = Engine.ListView
local Rect = Engine.Rect
local FPlainText = Engine.FunctionalPlainText

local UiControlFactory = {}

function UiControlFactory.createListView(logicalSize, defaultItemHeight, fixItemHeight, columns)
    return ListView.new(Engine.ToIntRect(0, 0, logicalSize.x, logicalSize.y), defaultItemHeight, fixItemHeight, columns)
end

function UiControlFactory.createFunctionalPlainText(textConfig)
    return FPlainText.new(textConfig, "")
end

function UiControlFactory.createFunctionalTextRow(logicalSize, textConfig)
    local root = Canvas.new(Engine.ToIntRect(0, 0, logicalSize.x, logicalSize.y))
    local text = UiControlFactory.createFunctionalPlainText(textConfig)
    root:addChild(text)
    return root, text
end

function UiControlFactory.layoutCenteredTextRow(root, text, top)
    local rootSize = root:getSize()
    local textSize = text:getSize()
    text:setPosition(sf.Vector2f.new((rootSize.x - textSize.x) / 2.0, top))
end

function UiControlFactory.createSelectionRect(size, windowSkin)
    return Rect.new(Engine.ToIntRect(0, 0, size.x, size.y), windowSkin, Rect.SelectionRectOpacityCurveKey)
end

return UiControlFactory
