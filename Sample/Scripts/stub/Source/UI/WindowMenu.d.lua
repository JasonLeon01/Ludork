---@meta Source.UI.WindowMenu

---@class Source.UI.WindowMenu: Source.UI.UiController
---@field new fun(model: Source.Windows.WindowMenu): Source.UI.WindowMenu
local WindowMenuUI = {}

---@return Engine.Window
function WindowMenuUI:getWindowFrame() end

---@return Engine.Canvas
function WindowMenuUI:getContent() end

---@return Engine.ScrollBox
function WindowMenuUI:getScrollBox() end

---@return Engine.ListView
function WindowMenuUI:getListView() end

return WindowMenuUI
