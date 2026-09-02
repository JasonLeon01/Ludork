---@meta Source.UI.Parts.WindowEquip.WindowEquipSelect

---@class Source.UI.Parts.WindowEquip.WindowEquipSelect: Source.UI.UiController
---@field new fun(model: table, instance: Engine.AssetInstance): Source.UI.Parts.WindowEquip.WindowEquipSelect
local WindowEquipSelectUI = {}

function WindowEquipSelectUI:attach() end

---@return Engine.Window
function WindowEquipSelectUI:getWindowFrame() end

---@return Engine.Canvas
function WindowEquipSelectUI:getContent() end

---@return Engine.ScrollBox
function WindowEquipSelectUI:getScrollBox() end

---@return Engine.ListView
function WindowEquipSelectUI:getListView() end

return WindowEquipSelectUI
