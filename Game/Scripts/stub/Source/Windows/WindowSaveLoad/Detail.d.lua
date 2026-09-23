---@meta

---@brief Save-file detail panel showing the current slot's screenshot and timestamp.
---
--- Renders the snapshot horizontally filling the content area at a 4:3 ratio
--- and displays the file's last-modified timestamp underneath. Loading, empty
--- and failed slots display a status message. Native previews are read in the background.
---@class Source.Windows.WindowSaveDetail.Controller: Internal.UIBase.UiController
---@field host            Source.Windows.WindowSaveDetail
---@field ui              Internal.UI.Parts.WindowSaveLoad.WindowSaveDetail
---@field _reader         Engine.SavePreviewReader
---@field _currentSlot    integer | nil
---@field _previewEnabled boolean
---@field _refreshElapsed number
---@field _thumbTexture   sf.Texture | nil
---@field _thumbImage     sf.Image | nil
---@field _requestClock   sf.Clock
---@field _reportTiming   boolean
---@field _lastError      string
---@field _timestamp      string | nil
local Controller = {}

---@brief Construct the detail panel.
---
function Controller:init() end

---@param enabled boolean
function Controller:setPreviewEnabled(enabled) end

---@brief Set the slot index to display, or ``nil`` to clear the panel.
---
--- - @param slot Zero-based slot index or ``nil``.
---@param slot integer | nil
function Controller:setSlot(slot) end

---@brief Force-refresh the panel against the current slot's save file.
function Controller:refresh() end

---@param deltaTime number
function Controller:onTick(deltaTime) end

function Controller:_requestPreview() end

function Controller:_displayPreview() end

---@param state string
function Controller:_showStatus(state) end

function Controller:dispose() end

---@param text string
function Controller:setTimestamp(text) end

---@param modificationTime number
function Controller:setModificationTime(modificationTime) end
