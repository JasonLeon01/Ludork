---@meta

---@brief Integrated floor teleporter window with visited-map list and preview.
---@class Source.Windows.WindowFloorTeleporter.Controller: Internal.UIBase.UiController
---@field host                       Source.Windows.WindowFloorTeleporter
---@field _inst                      Source.GameInstance.GameInstance | nil
---@field _onCloseCallback           function | nil
---@field _onConfirmCallback         fun(mapKey: string, telepoint: sf.Vector2u) | nil
---@field _clearPreviewCacheCallback function | nil
---@field ui                         Internal.UI.WindowFloorTeleporter
---@field _telepointEntriesCache     dict<tuple<any>, { [1]: sf.Vector2u, [2]: string } []>
---@field _transition                Internal.UIBase.WindowTransition
---@field _lastMapKey                string | nil
---@field _telepointIndexes          table<string, integer>
local Controller = {}

---@param inst                  Source.GameInstance.GameInstance
---@param loadPreview           function
---@param onConfirm             function | nil
---@param onClose               function | nil
---@param resolvePreviewMapPath function | nil
---@param clearPreviewCache     function | nil
function Controller:init(inst, loadPreview, onConfirm, onClose, resolvePreviewMapPath, clearPreviewCache) end

---@brief Get the floor map command window.
---
--- - @return The command window.
---@return Source.Windows.WindowFloorMapCommand
function Controller:getCommandWindow() end

---@brief Get the floor map preview window.
---
--- - @return The preview window.
---@return Source.Windows.WindowFloorMapPreview
function Controller:getPreviewWindow() end

---@brief Return whether the floor teleporter coordinator is visible.
---
--- - @return True while either selector stage is open.
---@return boolean
function Controller:getVisible() end

---@brief Open the floor teleporter with the floor list visible and the overlapping telepoint list hidden.
--- Select the current map's floor entry, falling back to the first entry if absent; reset the telepoint selector to its first entry.
---
--- - @param inst Optional current game instance to bind before opening.
---@param inst Source.GameInstance.GameInstance | nil
function Controller:open(inst) end

---@brief Close and deactivate both child windows.
---@param onHidden function | nil
function Controller:close(onHidden) end

---@brief Close the window via cancel input.
function Controller:closeByCancel() end

---@brief Refresh localised map and telepoint labels while preserving both selections and the active child window.
function Controller:refreshLocale() end

---@brief Hide the floor list and show the overlapping telepoint selector before moving input focus to it.
function Controller:activateTelepointSelector() end

---@brief Hide the telepoint selector and restore the overlapping visited-map list before moving input focus back to it.
---
--- - @param playCancelSE Whether to play the cancel sound.
---@param playCancelSE boolean | nil
function Controller:activateMapList(playCancelSE) end

---@brief Confirm the selected map telepoint.
function Controller:confirmSelectedTelepoint() end

---@brief Update selected telepoint index from the preview selector.
---
--- - @param index Current telepoint selector index, or nil.
---@param index integer | nil
function Controller:notifyTelepointIndexMaybeChanged(index) end

---@brief Get the selected telepoint for the selected map.
---
--- - @return Selected telepoint, or nil.
---@return sf.Vector2u | nil
function Controller:getCurrentTelepoint() end

---@brief Update the preview for the current command selection.
---
--- - @param index Current selected index, or nil.
---@param index integer | nil
function Controller:notifyMapIndexMaybeChanged(index) end

---@return Source.GameInstance.GameInstance
function Controller:getGameInstance() end

function Controller:clearPreviewCache() end

function Controller:notifyClosed() end

---@param mapKey    string
---@param telepoint sf.Vector2u
function Controller:confirmTelepoint(mapKey, telepoint) end

function Controller:hideImmediate() end

function Controller:refreshPreview() end

---@return { [1]: string, [2]: string } []
function Controller:getVisitedRegionEntries() end

---@param mapKey string
---@return Source.GameInstance.TelepointRecord[]
function Controller:getTelepointsForMap(mapKey) end

---@param mapKey     string | nil
---@param telepoints Source.GameInstance.TelepointRecord[]
---@return { [1]: sf.Vector2u, [2]: string } []
function Controller:getTelepointEntries(mapKey, telepoints) end

---@return table<string, boolean>
function Controller:getVisitedMapNames() end

---@param mapKey string
---@return string
function Controller:getMapDisplayName(mapKey) end

---@return boolean
function Controller:isBlocking() end
