---@meta Source.UI.Parts.WindowEquip.WindowEquipSlot

---@class Source.UI.Parts.WindowEquip.WindowEquipSlot: Source.UI.UiController
---@field new fun(model: table, instance: Engine.AssetInstance): Source.UI.Parts.WindowEquip.WindowEquipSlot
local WindowEquipSlotUI = {}

function WindowEquipSlotUI:attach() end

---@return Engine.Window
function WindowEquipSlotUI:getWindowFrame() end

---@return Engine.Canvas
function WindowEquipSlotUI:getContent() end

---@return Engine.ScrollBox
function WindowEquipSlotUI:getScrollBox() end

---@return Engine.ListView
function WindowEquipSlotUI:getListView() end

return WindowEquipSlotUI
