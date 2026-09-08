---@meta Source.Windows.WindowFloorTeleporter.Controller

---@class Source.Windows.WindowFloorTeleporter.Controller
---@field model                  Source.Windows.WindowFloorTeleporter
---@field new                    fun(model: Source.Windows.WindowFloorTeleporter, transition: Source.UI.WindowTransition): Source.Windows.WindowFloorTeleporter.Controller
---@field _telepointEntriesCache dict<tuple<any>, { [1]: sf.Vector2u, [2]: string } []>
---@field _transition            Source.UI.WindowTransition
---@field _lastMapKey            string | nil
---@field _telepointIndexes      table<string, integer>
local WindowFloorTeleporterController = {}

---@param model      Source.Windows.WindowFloorTeleporter
---@param transition Source.UI.WindowTransition
function WindowFloorTeleporterController:init(model, transition) end

function WindowFloorTeleporterController:open() end

---@param onHidden function | nil
function WindowFloorTeleporterController:close(onHidden) end

function WindowFloorTeleporterController:hideImmediate() end

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

---@return { [1]: string, [2]: string } []
function WindowFloorTeleporterController:getVisitedRegionEntries() end

---@param mapKey string
---@return Source.GameInstance.TelepointRecord[]
function WindowFloorTeleporterController:getTelepointsForMap(mapKey) end

---@param mapKey     string | nil
---@param telepoints Source.GameInstance.TelepointRecord[]
---@return { [1]: sf.Vector2u, [2]: string } []
function WindowFloorTeleporterController:getTelepointEntries(mapKey, telepoints) end

---@return table<string, boolean>
function WindowFloorTeleporterController:getVisitedMapNames() end

---@param mapKey string
---@return string
function WindowFloorTeleporterController:getMapDisplayName(mapKey) end

---@return boolean
function WindowFloorTeleporterController:isBlocking() end

return WindowFloorTeleporterController
