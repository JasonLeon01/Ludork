---@meta Source.Windows.WindowSaveLoad.Slot

---@brief Save-file slot list (1..MAX_SAVE_SLOTS) for load/save selection.
---@class Source.Windows.WindowSaveSlot: Source.Windows.Base.WindowSelectable
---@field MAX_SAVE_SLOTS integer
---@field new            fun(rect: sf.IntRect, owner: Source.Windows.WindowSaveLoad): Source.Windows.WindowSaveSlot
local WindowSaveSlot = {}

---@brief Construct the save slot list window.
---
--- - @param rect The window rectangle.
--- - @param owner The parent save/load UI coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowSaveLoad
function WindowSaveSlot:init(rect, owner) end

---@param deltaTime number
function WindowSaveSlot:onTick(deltaTime) end

function WindowSaveSlot:onReturn() end

return WindowSaveSlot
