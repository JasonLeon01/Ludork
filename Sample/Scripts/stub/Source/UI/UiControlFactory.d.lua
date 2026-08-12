---@meta Source.UI.UiControlFactory

---@class Source.UI.UiControlFactory.Module
local UiControlFactory = {}

---@param logicalSize sf.Vector2u
---@param defaultItemHeight integer
---@param fixItemHeight boolean
---@param columns integer
---@return Engine.ListView
function UiControlFactory.createListView(logicalSize, defaultItemHeight, fixItemHeight, columns) end

---@param textConfig Engine.PlainTextConfig
---@return Engine.FunctionalPlainText
function UiControlFactory.createFunctionalPlainText(textConfig) end

---@param logicalSize sf.Vector2u
---@param textConfig Engine.PlainTextConfig
---@return Engine.Canvas, Engine.FunctionalPlainText
function UiControlFactory.createFunctionalTextRow(logicalSize, textConfig) end

---@param root Engine.Canvas
---@param text Engine.FunctionalPlainText
---@param top number
function UiControlFactory.layoutCenteredTextRow(root, text, top) end

---@param size sf.Vector2u
---@param windowSkin sf.Image
---@return Engine.Rect
function UiControlFactory.createSelectionRect(size, windowSkin) end

return UiControlFactory
