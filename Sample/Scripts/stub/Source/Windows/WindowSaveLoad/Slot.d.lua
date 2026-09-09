---@meta

---@brief Save-file slot list (1..MAX_SAVE_SLOTS) for load/save selection.
---@class Source.Windows.WindowSaveSlot.Controller: Source.UIBase.UiController
---@field host           Source.Windows.WindowSaveSlot
---@field MAX_SAVE_SLOTS integer
---@field ui             Source.UI.Parts.WindowSaveLoad.WindowSaveSlot
---@field _rows          Source.UIBase.UiCollection<Source.Windows.WindowSaveLoad.WindowSaveSlotRow.Controller>
local Controller = {}

---@brief Construct the save slot list window.
---
--- - @param owner The parent save/load UI coordinator.
---@param owner Source.Windows.WindowSaveLoad
function Controller:init(owner) end

---@param deltaTime number
function Controller:onTick(deltaTime) end

---@param kwargs Engine.UiInputEventArguments
function Controller:onKeyDown(kwargs) end

function Controller:onReturn() end

function Controller:dispose() end

---@param slot integer
function Controller:confirmSlot(slot) end

function Controller:bind() end

function Controller:refresh() end
