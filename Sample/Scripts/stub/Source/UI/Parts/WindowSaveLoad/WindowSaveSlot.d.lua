---@meta Source.UI.Parts.WindowSaveLoad.WindowSaveSlot

---@param model    Source.Windows.WindowSaveSlot
---@param size     sf.Vector2i
---@param maxSlots integer
function WindowSaveSlotUI:init(model, size, maxSlots) end

function WindowSaveSlotUI:bind() end

function WindowSaveSlotUI:refresh() end

---@return Engine.Canvas
function WindowSaveSlotUI:prepare() end

function WindowSaveSlotUI:attach() end

---@return Engine.Window
function WindowSaveSlotUI:getWindowFrame() end

---@return Engine.Canvas
function WindowSaveSlotUI:getContent() end

---@return Engine.ListView
function WindowSaveSlotUI:getListView() end

function WindowSaveSlotUI:dispose() end

return WindowSaveSlotUI
