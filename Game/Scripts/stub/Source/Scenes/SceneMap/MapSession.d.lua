---@meta Source.Scenes.SceneMap.MapSession

---@class Source.Scenes.SceneMap.MapSession.PendingTeleporterTransfer
---@field player               Source.MapActors.Player.Player
---@field targetMap            string
---@field targetPosition       sf.Vector2i
---@field moveEnabled          boolean
---@field findNearest          boolean
---@field record               boolean
---@field targetStairDirection Enums.StairDirection | nil

---@class Source.Scenes.SceneMap.MapSession.PendingWorldTransfer
---@field player         Source.MapActors.Player.Player
---@field moveEnabled    boolean
---@field targetMap      string
---@field targetPosition sf.Vector2i

---@class Source.Scenes.SceneMap.MapSession
---@field private _scene                     Source.Scenes.SceneMap.SceneMap
---@field private _environment               Source.Scenes.SceneMap.Environment
---@field private _mapBuilder                Source.SceneComponents.SceneMapBuilder
---@field private _gameMap                   GameMap | nil
---@field private _mapFile                   string | nil
---@field private _currentRegion             string | nil
---@field private _pendingTeleporterTransfer Source.Scenes.SceneMap.MapSession.PendingTeleporterTransfer | nil
---@field private _pendingWorldTransfer      Source.Scenes.SceneMap.MapSession.PendingWorldTransfer | nil
---@field private _mapTransferInProgress     boolean
local MapSession = {}

---@param scene       Source.Scenes.SceneMap.SceneMap
---@param environment Source.Scenes.SceneMap.Environment
---@return Source.Scenes.SceneMap.MapSession
function MapSession.new(scene, environment) end

---@param scene       Source.Scenes.SceneMap.SceneMap
---@param environment Source.Scenes.SceneMap.Environment
function MapSession:init(scene, environment) end

---@return GameMap
function MapSession:getGameMap() end

---@return GameMap | nil
function MapSession:peekGameMap() end

---@return string | nil
function MapSession:getMapFile() end

---@return string
function MapSession:getCurrentRegionMap() end

--- Replace the runtime map after the scene has cancelled outgoing interactions.
--- The scene applies environment settings and publishes the map-change event after this returns.
---@param mapPath         string
---@param initialPosition sf.Vector2i | nil
---@return string, Global.GameMap.MapData | Global.WorldGameMap.WorldMapData
function MapSession:replaceMap(mapPath, initialPosition) end

--- Update the instance region and return a newly entered named region for its title.
---@return string | nil
function MapSession:updateRegion() end

--- Force the next destination application to reload the map and announce its region.
function MapSession:invalidateLoadedMap() end

function MapSession:cancelActorOperations() end

---@return boolean
function MapSession:isTransferInProgress() end

--- Cancel a queued transfer and restore the player captured when it was requested.
function MapSession:cancelPendingTransfers() end

--- Apply queued destinations only after the previous rendered frame has been frozen.
function MapSession:processPendingTransfers() end

function MapSession:dispose() end

---@param mapKey string
---@return string
function MapSession:resolveRegionMapPath(mapKey) end

---@param mapKey              string
---@param telepoint           sf.Vector2u
---@param previewSize         integer
---@param previewScale        number
---@param showTelepointMarker boolean
---@return sf.Texture
function MapSession:buildFloorMapPreview(mapKey, telepoint, previewSize, previewScale, showTelepointMarker) end

---@param mapKey string
---@return string, integer, integer
function MapSession:getFloorMapPreviewState(mapKey) end

function MapSession:clearFloorMapPreviewCache() end

---@param mapFile string
---@return string | nil
function MapSession.FindRegionForMap(mapFile) end

---@param mapPath string
---@return string
function MapSession.NormaliseRegionMapName(mapPath) end

---@private
---@param teleporter Source.MapActors.Teleporter.Teleporter
---@return boolean
function MapSession:_canRequestTeleporterTransfer(teleporter) end

---@param teleporter Source.MapActors.Teleporter.Teleporter
---@param step       integer
---@return boolean
function MapSession:requestFloorStep(teleporter, step) end

---@param teleporter Source.MapActors.Teleporter.Teleporter
---@param mapPath    string
---@param position   sf.Vector2i
---@param record     boolean | nil
---@return boolean
function MapSession:requestMapTransfer(teleporter, mapPath, position, record) end

---@private
---@param teleporter           Source.MapActors.Teleporter.Teleporter
---@param targetMap            string
---@param targetPosition       sf.Vector2i
---@param findNearest          boolean
---@param record               boolean
---@param targetStairDirection Enums.StairDirection | nil
---@return boolean
function MapSession:_startTeleporterTransfer(
    teleporter, targetMap, targetPosition, findNearest, record, targetStairDirection
) end

---@param targetMap             string
---@param targetPosition        sf.Vector2i
---@param moveEnabled           boolean
---@param findNearest           boolean
---@param record                boolean
---@param targetStairDirection? Enums.StairDirection
---@return boolean
function MapSession:requestTeleporterTransfer(
    targetMap, targetPosition, moveEnabled, findNearest, record, targetStairDirection
) end

---@private
---@param transferData Source.Scenes.SceneMap.MapSession.PendingTeleporterTransfer
function MapSession:_processTeleporterTransfer(transferData) end

---@private
---@param player      Source.MapActors.Player.Player
---@param moveEnabled boolean
function MapSession:_cancelTeleporterTransfer(player, moveEnabled) end

---@private
---@param targetMap       string
---@param targetPosition  sf.Vector2i | nil
---@param blockTransition boolean
function MapSession:_applyMapDestination(targetMap, targetPosition, blockTransition) end

---@private
---@param targetMap      string
---@param targetPosition sf.Vector2i
function MapSession:_queueWorldTransfer(targetMap, targetPosition) end

---@private
---@param transferData Source.Scenes.SceneMap.MapSession.PendingWorldTransfer
function MapSession:_processWorldTransfer(transferData) end

---@param mapPath          string
---@param pos              sf.Vector2i | nil
---@param blockTransition? boolean
function MapSession:gotoMapAndPos(mapPath, pos, blockTransition) end

---@return boolean
function MapSession:tryCenterSymmetricTeleport() end

---@param step integer
---@return boolean
function MapSession:tryAdjacentFloorSamePos(step) end

---@private
---@param mapPath  string
---@param actor    Engine.Actor
---@param position sf.Vector2i
---@return boolean
function MapSession:_isMapPositionPassable(mapPath, actor, position) end

---@param actor Engine.Actor
function MapSession:recordAddedActor(actor) end

---@param actor    Engine.Actor
---@param position sf.Vector2i | nil
function MapSession:recordActorPosition(actor, position) end

---@param actor Engine.Actor
function MapSession:recordDestroyedActor(actor) end

---@param actorTag string
function MapSession:recordDestroyedActorTag(actorTag) end

---@param layerName string
---@param positions sf.Vector2i[]
function MapSession:recordTerrainDestructions(layerName, positions) end

return MapSession
