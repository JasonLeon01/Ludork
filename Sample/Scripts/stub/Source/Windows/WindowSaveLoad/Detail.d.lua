---@meta Source.Windows.WindowSaveLoad.Detail

---@brief Save-file detail panel showing the current slot's screenshot and timestamp.
---
--- Renders the snapshot horizontally filling the content area at a 4:3 ratio
--- and displays the file's last-modified timestamp underneath. When the slot
--- has no save file on disk, both the snapshot and timestamp stay hidden.
---@class Source.Windows.WindowSaveDetail: Source.Windows.Base.WindowBase
---@field new fun(rect: sf.IntRect): Source.Windows.WindowSaveDetail
local WindowSaveDetail = {}

---@brief Construct the detail panel.
---
--- - @param rect The window rectangle (expected 256x256).
---@param rect sf.IntRect
function WindowSaveDetail:init(rect) end

---@brief Set the slot index to display, or ``nil`` to clear the panel.
---
--- - @param slot Zero-based slot index or ``nil``.
---@param slot integer | nil
function WindowSaveDetail:setSlot(slot) end

---@brief Force-refresh the panel against the current slot's save file.
function WindowSaveDetail:refresh() end

---@param deltaTime number
function WindowSaveDetail:onTick(deltaTime) end

function WindowSaveDetail:dispose() end

return WindowSaveDetail
