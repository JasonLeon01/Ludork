---@meta Source.Windows.WindowSaveLoad.Slot

---@brief Save-file slot list (1..MAX_SAVE_SLOTS) for load/save selection.
---@class Source.Windows.WindowSaveSlot: Source.Windows.Base.WindowSelectable
---@field new            fun(rect: sf.IntRect, owner: Source.Windows.WindowSaveLoad, instance?: Engine.AssetInstance): Source.Windows.WindowSaveSlot
---@field MAX_SAVE_SLOTS integer
---@field new            fun(rect: sf.IntRect, owner: Source.Windows.WindowSaveLoad): Source.Windows.WindowSaveSlot
local WindowSaveSlot = {}

---@brief Construct the save slot list window.
---
--- - @param rect The window rectangle.
--- - @param owner The parent save/load UI coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowSaveLoad
function WindowSaveSlot:init(rect, owner, instance) end

---@param deltaTime number
function WindowSaveSlot:onTick(deltaTime) end

---@param kwargs table
function WindowSaveSlot:onKeyDown(kwargs) end

function WindowSaveSlot:onReturn() end

function WindowSaveSlot:dispose() end

return WindowSaveSlot
