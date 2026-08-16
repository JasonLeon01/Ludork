---@meta Source.Windows.WindowFloorTeleporter

---@class Source.Windows.WindowFloorMapCommandController: Source.Windows.WindowCommand.Controller
---@field _mapKeys string[]
local WindowFloorMapCommandController = {}

function WindowFloorMapCommandController:init(model, size, rowHeight, columns) end

---@param entries table
function WindowFloorMapCommandController:refreshMaps(entries) end

---@return string | nil
function WindowFloorMapCommandController:getCurrentMapKey() end

function WindowFloorMapCommandController:afterTick() end

---@return boolean
function WindowFloorMapCommandController:handleKeyDown() end

---@param kwargs table
---@return boolean
function WindowFloorMapCommandController:handleMouseButtonDown(kwargs) end

--- @brief Command list displaying visited maps in the current region.
---@class Source.Windows.WindowFloorMapCommand: Source.Windows.WindowCommand
---@field _owner Source.Windows.WindowFloorTeleporter
---@field _mapController Source.Windows.WindowFloorMapCommandController
---@field _mapKeys string[]
local WindowFloorMapCommand = {}

--- @brief Construct the floor map command list.
---
--- - @param rect The command list window rectangle.
--- - @param owner The parent floor teleporter coordinator.
---@param rect  sf.IntRect
---@param owner Source.Windows.WindowFloorTeleporter
function WindowFloorMapCommand:init(rect, owner) end

--- @brief Rebuild the list from map key/name pairs.
---
--- - @param entries Region map entries to display.
---@param entries table
function WindowFloorMapCommand:refreshMaps(entries) end

--- @brief Get the selected region map key.
---
--- - @return The selected map key, or nil when no map is selected.
---@return string | nil
function WindowFloorMapCommand:getCurrentMapKey() end

---@param deltaTime number
function WindowFloorMapCommand:onTick(deltaTime) end

---@param kwargs table
function WindowFloorMapCommand:onKeyDown(kwargs) end

---@param kwargs table
---@return boolean
function WindowFloorMapCommand:onMouseButtonDown(kwargs) end

function WindowFloorMapCommand:onReturn() end

--- @brief Right-side preview panel and telepoint selector for the selected map.
---@class Source.Windows.WindowFloorMapPreview: Source.Windows.Base.WindowSelectable
local WindowFloorMapPreview = {}

--- @brief Construct the map preview panel.
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

--- @brief Refresh the preview when the selected map changes.
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

---@param kwargs table
---@return boolean
function WindowFloorMapPreview:onMouseButtonDown(kwargs) end

function WindowFloorMapPreview:onReturn() end

local WindowFloorTeleporterController = {}

---@param model Source.Windows.WindowFloorTeleporter
function WindowFloorTeleporterController:init(model) end

---@param inst Source.GameInstance.GameInstance | nil
function WindowFloorTeleporterController:open(inst) end

function WindowFloorTeleporterController:close() end

function WindowFloorTeleporterController:closeByCancel() end

function WindowFloorTeleporterController:refreshLocale() end

function WindowFloorTeleporterController:activateTelepointSelector() end

---@param playCancelSE boolean | nil
function WindowFloorTeleporterController:activateMapList(playCancelSE) end

function WindowFloorTeleporterController:confirmSelectedTelepoint() end

---@param index integer | nil
function WindowFloorTeleporterController:notifyTelepointIndexMaybeChanged(index) end

---@return sf.Vector2u | nil
function WindowFloorTeleporterController:getCurrentTelepoint() end

---@param index integer | nil
function WindowFloorTeleporterController:notifyMapIndexMaybeChanged(index) end

function WindowFloorTeleporterController:refreshPreview() end

---@return table
function WindowFloorTeleporterController:getVisitedRegionEntries() end

---@param mapKey string
---@return sf.Vector2u[]
function WindowFloorTeleporterController:getTelepointsForMap(mapKey) end

---@param mapKey     string | nil
---@param telepoints sf.Vector2u[]
---@return table
function WindowFloorTeleporterController:getTelepointEntries(mapKey, telepoints) end

---@return table
function WindowFloorTeleporterController:getVisitedMapNames() end

---@param mapKey string
---@return string
function WindowFloorTeleporterController:getMapDisplayName(mapKey) end

---@param mapKey    string
---@param telepoint sf.Vector2u
---@param index     integer
---@return string
function WindowFloorTeleporterController:formatTelepointName(mapKey, telepoint, index) end

---@param mapPath string
---@return string
function WindowFloorTeleporterController.NormaliseMapName(mapPath) end

---@param mapName string
---@return string
function WindowFloorTeleporterController.FormatMapName(mapName) end

---@return sf.IntRect, sf.IntRect
function WindowFloorTeleporterController.GetDefaultRects() end

--- @brief Integrated floor teleporter window with visited-map list and preview.
---@class Source.Windows.WindowFloorTeleporter
local WindowFloorTeleporter = {}

--- @brief Construct the floor teleporter coordinator.
---
--- - @param inst Game instance used for region and visited-map state.
--- - @param listRect Rectangle for the command list.
--- - @param previewRect Rectangle for the map preview.
--- - @param loadPreview Callback that builds preview textures.
--- - @param onConfirm Callback invoked when the selected map and telepoint are confirmed.
--- - @param onClose Callback invoked after the window closes.
--- - @param getTelepointTag Callback that finds the telepoint actor tag.
--- - @param resolvePreviewMapPath Callback that resolves a map key for caching.
--- - @param clearPreviewCache Callback that clears cached preview maps.
---@param inst                  Source.GameInstance.GameInstance
---@param listRect              sf.IntRect
---@param previewRect           sf.IntRect
---@param loadPreview           function
---@param onConfirm             function | nil
---@param onClose               function | nil
---@param getTelepointTag       function | nil
---@param resolvePreviewMapPath function | nil
---@param clearPreviewCache     function | nil
function WindowFloorTeleporter:init( inst, listRect, previewRect, loadPreview, onConfirm, onClose, getTelepointTag, resolvePreviewMapPath, clearPreviewCache ) end

--- @brief Get the floor map command window.
---
--- - @return The command window.
---@return Source.Windows.WindowFloorMapCommand
function WindowFloorTeleporter:getCommandWindow() end

--- @brief Get the floor map preview window.
---
--- - @return The preview window.
---@return Source.Windows.WindowFloorMapPreview
function WindowFloorTeleporter:getPreviewWindow() end

--- @brief Return whether the floor teleporter is visible.
---
--- - @return True when the list window is visible.
---@return boolean
function WindowFloorTeleporter:getVisible() end

--- @brief Open and refresh the floor teleporter window.
---
--- - @param inst Optional current game instance to bind before opening.
---@param inst Source.GameInstance.GameInstance | nil
function WindowFloorTeleporter:open(inst) end

--- @brief Close and deactivate both child windows.
function WindowFloorTeleporter:close() end

--- @brief Close the window via cancel input.
function WindowFloorTeleporter:closeByCancel() end

--- @brief Refresh localised map and telepoint labels while preserving both selections and the active child window.
function WindowFloorTeleporter:refreshLocale() end

--- @brief Move input focus from the map list to the telepoint selector.
function WindowFloorTeleporter:activateTelepointSelector() end

--- @brief Move input focus back to the visited-map list.
---
--- - @param playCancelSE Whether to play the cancel sound.
---@param playCancelSE boolean | nil
function WindowFloorTeleporter:activateMapList(playCancelSE) end

--- @brief Confirm the selected map telepoint.
function WindowFloorTeleporter:confirmSelectedTelepoint() end

--- @brief Update selected telepoint index from the preview selector.
---
--- - @param index Current telepoint selector index, or nil.
---@param index integer | nil
function WindowFloorTeleporter:notifyTelepointIndexMaybeChanged(index) end

--- @brief Get the selected telepoint for the selected map.
---
--- - @return Selected telepoint, or nil.
---@return sf.Vector2u | nil
function WindowFloorTeleporter:getCurrentTelepoint() end

--- @brief Update the preview for the current command selection.
---
--- - @param index Current selected index, or nil.
---@param index integer | nil
function WindowFloorTeleporter:notifyMapIndexMaybeChanged(index) end

---@class Source.Windows.WindowFloorTeleporterExports
---@field WindowFloorMapCommand Source.Windows.WindowFloorMapCommand & Class.ClassType<Source.Windows.WindowFloorMapCommand>
---@field WindowFloorMapPreview Source.Windows.WindowFloorMapPreview & Class.ClassType<Source.Windows.WindowFloorMapPreview>
---@field WindowFloorTeleporter Source.Windows.WindowFloorTeleporter & Class.ClassType<Source.Windows.WindowFloorTeleporter>
local WindowFloorTeleporterExports = {}

--- @brief Calculate centred default rectangles for the floor teleporter UI.
---
--- - @return A pair containing list and preview window rectangles.
---@return sf.IntRect, sf.IntRect
function WindowFloorTeleporterExports.GetDefaultFloorTeleporterRects() end

return WindowFloorTeleporterExports
