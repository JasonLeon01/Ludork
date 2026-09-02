local Engine = require("Engine")

local Canvas = Engine.Canvas
local ListView = Engine.ListView
local Rect = Engine.Rect
local ScrollBox = Engine.ScrollBox
local FPlainText = Engine.FunctionalPlainText

local UiControlFactory = {}

function UiControlFactory.CreateListView(logicalSize, defaultItemHeight, fixItemHeight, columns)
    return ListView.new(Engine.ToIntRect(0, 0, logicalSize.x, logicalSize.y), defaultItemHeight, fixItemHeight, columns)
end

function UiControlFactory.CreateScrollBox(logicalSize, windowSkin)
    return ScrollBox.new(sf.Vector2f.new(logicalSize.x, logicalSize.y), windowSkin)
end

function UiControlFactory.CreateFunctionalPlainText(textConfig)
    return FPlainText.new(textConfig, "")
end

function UiControlFactory.CreateFunctionalTextRow(logicalSize, textConfig)
    local root = Canvas.new(Engine.ToIntRect(0, 0, logicalSize.x, logicalSize.y))
    local text = UiControlFactory.CreateFunctionalPlainText(textConfig)
    root:addChild(text)
    return root, text
end

function UiControlFactory.LayoutCenteredTextRow(root, text, top)
    local rootSize = root:getSize()
    local textBounds = text:getLocalBounds()
    local positionX = (rootSize.x - textBounds.size.x) / 2.0 - textBounds.position.x
    text:setPosition(sf.Vector2f.new(positionX, top))
end

function UiControlFactory.CreateSelectionRect(size, windowSkin)
    return Rect.new(Engine.ToIntRect(0, 0, size.x, size.y), windowSkin, Rect.SelectionRectOpacityCurveKey)
end

return UiControlFactory
