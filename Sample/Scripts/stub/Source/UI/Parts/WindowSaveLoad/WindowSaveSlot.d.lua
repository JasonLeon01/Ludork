---@meta Source.UI.Parts.WindowSaveLoad.WindowSaveSlot

---@class Source.UI.Parts.WindowSaveLoad.WindowSaveSlot: Source.UI.UiController
---@field new fun(model: table, size: sf.Vector2i, maxSlots: integer, instance?: Engine.AssetInstance): Source.UI.Parts.WindowSaveLoad.WindowSaveSlot
local WindowSaveSlotUI = {}

---@param model    Source.Windows.WindowSaveSlot
---@param size     sf.Vector2i
---@param maxSlots integer
---@param instance Engine.AssetInstance | nil
function WindowSaveSlotUI:init(model, size, maxSlots, instance) end

function WindowSaveSlotUI:bind() end

function WindowSaveSlotUI:refresh() end

---@return Engine.Canvas
function WindowSaveSlotUI:prepare() end

---@param nested boolean | nil
function WindowSaveSlotUI:attach(nested) end

---@return Engine.Window
function WindowSaveSlotUI:getWindowFrame() end

---@return Engine.Canvas
function WindowSaveSlotUI:getContent() end

---@return Engine.ListView
function WindowSaveSlotUI:getListView() end

---@return Engine.ScrollBox
function WindowSaveSlotUI:getScrollBox() end

function WindowSaveSlotUI:dispose() end

return WindowSaveSlotUI
