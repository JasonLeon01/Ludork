---@meta Source.Windows.WindowFloorTeleporter.Controller

---@class Source.Windows.WindowFloorTeleporter.Controller
---@field model Source.Windows.WindowFloorTeleporter
---@field new   fun(model: Source.Windows.WindowFloorTeleporter): Source.Windows.WindowFloorTeleporter.Controller
local WindowFloorTeleporterController = {}

---@param model Source.Windows.WindowFloorTeleporter
function WindowFloorTeleporterController:init(model) end

---@param inst Source.GameInstance.GameInstance | nil
function WindowFloorTeleporterController:open(inst) end

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

return WindowFloorTeleporterController
