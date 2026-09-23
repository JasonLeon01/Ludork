---@meta

---@brief Save-file slot list (1..MAX_SAVE_SLOTS) for load/save selection.
---@class Source.Windows.WindowSaveSlot.Controller: Internal.UIBase.UiController
---@field host           Source.Windows.WindowSaveSlot
---@field MAX_SAVE_SLOTS integer
---@field ui             Internal.UI.Parts.WindowSaveLoad.WindowSaveSlot
---@field _rows          Internal.UIBase.UiCollection<Source.Windows.WindowSaveLoad.WindowSaveSlotRow.Controller>
---@field _buildClock    sf.Clock
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

---@return boolean
function Controller:isReady() end

function Controller:_buildRows() end

function Controller:refresh() end
