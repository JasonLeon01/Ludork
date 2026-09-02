---@meta Source.UI.UiControlFactory

---@class Source.UI.UiControlFactory.Module
local UiControlFactory = {}

---@param logicalSize       sf.Vector2u
---@param defaultItemHeight integer
---@param fixItemHeight     boolean
---@param columns           integer
---@return Engine.ListView
function UiControlFactory.CreateListView(logicalSize, defaultItemHeight, fixItemHeight, columns) end

---@param logicalSize sf.Vector2u
---@param windowSkin  sf.Image
---@return Engine.ScrollBox
function UiControlFactory.CreateScrollBox(logicalSize, windowSkin) end

---@param textConfig Engine.PlainTextConfig
---@return Engine.FunctionalPlainText
function UiControlFactory.CreateFunctionalPlainText(textConfig) end

---@param logicalSize sf.Vector2u
---@param textConfig  Engine.PlainTextConfig
---@return Engine.Canvas, Engine.FunctionalPlainText
function UiControlFactory.CreateFunctionalTextRow(logicalSize, textConfig) end

---@param root Engine.Canvas
---@param text Engine.FunctionalPlainText
---@param top  number
function UiControlFactory.LayoutCenteredTextRow(root, text, top) end

---@param size       sf.Vector2u
---@param windowSkin sf.Image
---@return Engine.Rect
function UiControlFactory.CreateSelectionRect(size, windowSkin) end

return UiControlFactory
