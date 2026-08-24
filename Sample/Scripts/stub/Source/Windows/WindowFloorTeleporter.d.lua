---@meta Source.Windows.WindowFloorTeleporter

---@brief Integrated floor teleporter window with visited-map list and preview.
---@class Source.Windows.WindowFloorTeleporter
---@field new fun(inst: Source.GameInstance.GameInstance, listRect: sf.IntRect, previewRect: sf.IntRect, loadPreview: function, onConfirm?: function, onClose?: function, getTelepointTag?: function, resolvePreviewMapPath?: function, clearPreviewCache?: function): Source.Windows.WindowFloorTeleporter
local WindowFloorTeleporter = {}

---@brief Construct the floor teleporter coordinator.
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
function WindowFloorTeleporter:init(
    inst, listRect, previewRect, loadPreview, onConfirm, onClose, getTelepointTag, resolvePreviewMapPath,
    clearPreviewCache
) end

---@brief Get the floor map command window.
---
--- - @return The command window.
---@return Source.Windows.WindowFloorMapCommand
function WindowFloorTeleporter:getCommandWindow() end

---@brief Get the floor map preview window.
---
--- - @return The preview window.
---@return Source.Windows.WindowFloorMapPreview
function WindowFloorTeleporter:getPreviewWindow() end

---@brief Return whether the floor teleporter is visible.
---
--- - @return True when the list window is visible.
---@return boolean
function WindowFloorTeleporter:getVisible() end

---@brief Open and refresh the floor teleporter window.
---
--- - @param inst Optional current game instance to bind before opening.
---@param inst Source.GameInstance.GameInstance | nil
function WindowFloorTeleporter:open(inst) end

---@brief Close and deactivate both child windows.
function WindowFloorTeleporter:close() end

---@brief Close the window via cancel input.
function WindowFloorTeleporter:closeByCancel() end

---@brief Refresh localised map and telepoint labels while preserving both selections and the active child window.
function WindowFloorTeleporter:refreshLocale() end

---@brief Move input focus from the map list to the telepoint selector.
function WindowFloorTeleporter:activateTelepointSelector() end

---@brief Move input focus back to the visited-map list.
---
--- - @param playCancelSE Whether to play the cancel sound.
---@param playCancelSE boolean | nil
function WindowFloorTeleporter:activateMapList(playCancelSE) end

---@brief Confirm the selected map telepoint.
function WindowFloorTeleporter:confirmSelectedTelepoint() end

---@brief Update selected telepoint index from the preview selector.
---
--- - @param index Current telepoint selector index, or nil.
---@param index integer | nil
function WindowFloorTeleporter:notifyTelepointIndexMaybeChanged(index) end

---@brief Get the selected telepoint for the selected map.
---
--- - @return Selected telepoint, or nil.
---@return sf.Vector2u | nil
function WindowFloorTeleporter:getCurrentTelepoint() end

---@brief Update the preview for the current command selection.
---
--- - @param index Current selected index, or nil.
---@param index integer | nil
function WindowFloorTeleporter:notifyMapIndexMaybeChanged(index) end

---@brief Calculate centred default rectangles for the floor teleporter UI.
---
--- - @return A pair containing list and preview window rectangles.
---@return sf.IntRect, sf.IntRect
function WindowFloorTeleporter.GetDefaultFloorTeleporterRects() end

return WindowFloorTeleporter
