---@meta Source.Windows.WindowFloorTeleporter.Preview

---@brief Right-side preview panel and telepoint selector for the selected map.
---@class Source.Windows.WindowFloorMapPreview: Source.Windows.Base.WindowSelectable
---@field new fun(rect: sf.IntRect, owner: Source.Windows.WindowFloorTeleporter, loadPreview: function, resolvePreviewMapPath?: function): Source.Windows.WindowFloorMapPreview
local WindowFloorMapPreview = {}

---@brief Construct the map preview panel.
---
--- - @param rect The preview window rectangle.
--- - @param owner The parent floor teleporter coordinator.
--- - @param loadPreview Callback that builds a preview texture for a map key.
--- - @param resolvePreviewMapPath Callback that resolves a map key for caching.
---@param rect                  sf.IntRect
---@param owner                 Source.Windows.WindowFloorTeleporter
---@param loadPreview           function
---@param resolvePreviewMapPath function | nil
function WindowFloorMapPreview:init(rect, owner, loadPreview, resolvePreviewMapPath) end

function WindowFloorMapPreview:clearPreviewCache() end

---@param active boolean
function WindowFloorMapPreview:setActive(active) end

---@brief Refresh the preview when the selected map changes.
---
--- - @param mapKey Selected region map key, or nil.
--- - @param entries Telepoint and display-name pairs.
--- - @param selectedIndex Selected telepoint index.
---@param mapKey        string | nil
---@param entries       table
---@param selectedIndex integer
function WindowFloorMapPreview:setMapKeyAndTelepoints(mapKey, entries, selectedIndex) end

---@param deltaTime number
function WindowFloorMapPreview:onTick(deltaTime) end

---@param kwargs table
function WindowFloorMapPreview:onKeyDown(kwargs) end

function WindowFloorMapPreview:onReturn() end

return WindowFloorMapPreview
