---@meta

---@brief Right-side preview panel and telepoint selector for the selected map.
---@class Source.Windows.WindowFloorMapPreview.Controller: Source.UIBase.UiController
---@field host                   Source.Windows.WindowFloorMapPreview
---@field ui                     Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapPreview
---@field _mapKey                string | nil
---@field _telepoints            sf.Vector2u[]
---@field _currentListKey        tuple<any> | nil
---@field _currentPreviewKey     tuple<any> | nil
---@field _previewTextureCache   dict<tuple<any>, sf.Texture>
---@field _loadPreview           function
---@field _resolvePreviewMapPath function | nil
---@field _rows                  Source.UIBase.UiCollection<Source.UIBase.CommandRow.Controller>
local Controller = {}

---@brief Construct the map preview panel.
---
--- - @param owner The parent floor teleporter coordinator.
--- - @param loadPreview Callback that builds a preview texture for a map key.
--- - @param resolvePreviewMapPath Callback returning the resolved map path and current visibility revision for caching.
---@param owner                 Source.Windows.WindowFloorTeleporter
---@param loadPreview           function
---@param resolvePreviewMapPath function | nil
function Controller:init(owner, loadPreview, resolvePreviewMapPath) end

function Controller:clearPreviewCache() end

---@param active boolean
function Controller:setActive(active) end

---@brief Refresh the preview when the selected map changes.
---
--- - @param mapKey Selected region map key, or nil.
--- - @param entries Telepoint and display-name pairs.
--- - @param selectedIndex Selected telepoint index.
---@param mapKey        string | nil
---@param entries       table
---@param selectedIndex integer
function Controller:setMapKeyAndTelepoints(mapKey, entries, selectedIndex) end

---@param deltaTime number
function Controller:onTick(deltaTime) end

---@param kwargs Engine.UiInputEventArguments
function Controller:onKeyDown(kwargs) end

function Controller:onReturn() end

---@param index integer
function Controller:_setPointerIndex(index) end

function Controller:confirmSelectedTelepoint() end

---@param index integer | nil
function Controller:notifyTelepointIndexMaybeChanged(index) end

function Controller:refresh() end

---@param active    boolean
---@param wasActive boolean
function Controller:onActiveChanged(active, wasActive) end

---@param previousIndex integer | nil
function Controller:afterSelectionUpdate(previousIndex) end

---@param entries table
function Controller:rebuildTelepointList(entries) end

function Controller:refreshSelectedPreview() end

---@return sf.Vector2u | nil
function Controller:getSelectedTelepoint() end

function Controller:hidePreview() end
