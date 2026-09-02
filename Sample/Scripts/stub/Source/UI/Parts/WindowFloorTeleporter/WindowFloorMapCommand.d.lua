---@meta Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapCommand

---@class Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapCommand: Source.UI.UiController
---@field new fun(model: Source.Windows.WindowFloorMapCommand, instance: Engine.AssetInstance): Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapCommand
local WindowFloorMapCommandUI = {}

function WindowFloorMapCommandUI:attach() end

---@return Engine.Window
function WindowFloorMapCommandUI:getWindowFrame() end

---@return Engine.Canvas
function WindowFloorMapCommandUI:getContent() end

---@return Engine.ScrollBox
function WindowFloorMapCommandUI:getScrollBox() end

---@return Engine.ListView
function WindowFloorMapCommandUI:getListView() end

return WindowFloorMapCommandUI
