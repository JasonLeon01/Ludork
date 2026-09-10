---@meta

---@brief Save-file detail panel showing the current slot's screenshot and timestamp.
---
--- Renders the snapshot horizontally filling the content area at a 4:3 ratio
--- and displays the file's last-modified timestamp underneath. When the slot
--- has no save file on disk, both the snapshot and timestamp stay hidden.
---@class Source.Windows.WindowSaveDetail.Controller: Source.UIBase.UiController
---@field host Source.Windows.WindowSaveDetail
---@field ui   Source.UI.Parts.WindowSaveLoad.WindowSaveDetail
local Controller = {}

---@brief Construct the detail panel.
---
function Controller:init() end

---@brief Set the slot index to display, or ``nil`` to clear the panel.
---
--- - @param slot Zero-based slot index or ``nil``.
---@param slot integer | nil
function Controller:setSlot(slot) end

---@brief Force-refresh the panel against the current slot's save file.
function Controller:refresh() end

---@param deltaTime number
function Controller:onTick(deltaTime) end

function Controller:dispose() end

---@param text string
function Controller:setTimestamp(text) end

---@param modificationTime number
function Controller:setModificationTime(modificationTime) end
