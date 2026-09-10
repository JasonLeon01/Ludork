---@meta Source.SceneComponents.MapBuilder
---@class Source.SceneComponents.MapAudioData
---@field bgm       string | nil
---@field bgs       string | nil
---@field bgmFilter Source.SceneComponents.MusicFilterData | nil
---@field bgsFilter Source.SceneComponents.MusicFilterData | nil

---@class Source.SceneComponents.MapEnvironmentData: Source.SceneComponents.MapAudioData
---@field fog        string | nil
---@field fogPower   number | nil
---@field fogOx      number | nil
---@field fogOy      number | nil
---@field fogDistort number | nil
---@field panorama   string | nil

---@alias Source.SceneComponents.SerializedTileCell integer | lightuserdata
---@alias Source.SceneComponents.SerializedAutoTileCell integer | string | lightuserdata

---@class Source.SceneComponents.SerializedTileRow
---@field n         integer | nil
---@field [integer] Source.SceneComponents.SerializedTileCell

---@class Source.SceneComponents.SerializedAutoTileRow
---@field n         integer | nil
---@field [integer] Source.SceneComponents.SerializedAutoTileCell

---@class Source.SceneComponents.MapLayerData
---@field layerName    string
---@field layerTileset string
---@field tiles        Source.SceneComponents.SerializedTileRow[]
---@field autoTiles    Source.SceneComponents.SerializedAutoTileRow[] | nil
---@field shaderPath   string | nil

---@class Source.SceneComponents.MapLightData
---@field position  number[] | nil
---@field color     number[] | nil
---@field radius    number | nil
---@field intensity number | nil

---@class Source.SceneComponents.MapDataBase: Source.SceneComponents.MapEnvironmentData
---@field type              "map" | nil
---@field mapName           string
---@field width             integer
---@field height            integer
---@field layerOrder        string[]
---@field layers            table<string, Source.SceneComponents.MapLayerData>
---@field BPClassVarChanged table<string, table<string, Source.Data.ClassVarValue>> | nil

---@class Source.SceneComponents.SerializedMapData: Source.SceneComponents.MapDataBase
---@field actors       table<string, Source.Data.SerializedActorData[]> | nil
---@field ambientLight integer[] | nil
---@field lights       Source.SceneComponents.MapLightData[] | nil

---@class Source.SceneComponents.MapData: Source.SceneComponents.MapDataBase
---@field actors       table<string, Source.Data.ActorData[]>
---@field ambientLight sf.Color
---@field lights       GlobalCore.Light[]

---@class Source.SceneComponents.WorldRegionEnvironmentData: Source.SceneComponents.MapEnvironmentData
---@field ambientLight sf.Color

---@class Source.SceneComponents.WorldRegionData: Global.WorldGeometry.CellRect
---@field index                 integer
---@field map                   string
---@field path                  string
---@field payload               Global.WorldGameMap.RegionPayload | nil
---@field sleepTime             number | nil
---@field wasActive             boolean
---@field wakeTags              table<string, number> | nil
---@field publishState          Global.WorldGameMap.RegionPublishState | nil
---@field backgroundBuilder     Global.WorldGameMap.RegionBuildState | nil
---@field geometryRevision      integer | nil
---@field lightingRevision      integer | nil
---@field activeChunkGeneration integer | nil

---@class Source.SceneComponents.WorldLayerValidationState
---@field layerOrders       table<integer, string[] | nil>
---@field loadedRegionCount integer

---@class Source.SceneComponents.WorldActorDescriptorBase
---@field layer    string
---@field position sf.Vector2i

---@class Source.SceneComponents.WorldAuthoredActorDescriptor: Source.SceneComponents.WorldActorDescriptorBase
---@field kind       "authored"
---@field normalised Source.Data.ActorData

---@class Source.SceneComponents.WorldAddedActorDescriptor: Source.SceneComponents.WorldActorDescriptorBase
---@field kind   "added"
---@field record Source.GameInstance.AddedActorRecord

---@class Source.SceneComponents.WorldMovedActorDescriptor: Source.SceneComponents.WorldActorDescriptorBase
---@field kind   "moved"
---@field record Source.GameInstance.WorldMovedActorRecord

---@alias Source.SceneComponents.WorldActorDescriptor Source.SceneComponents.WorldAuthoredActorDescriptor | Source.SceneComponents.WorldAddedActorDescriptor | Source.SceneComponents.WorldMovedActorDescriptor

---@class Source.SceneComponents.WorldTileBlockRow
---@field n         integer
---@field [integer] integer

---@class Source.SceneComponents.WorldTileBlock
---@field n         integer
---@field [integer] Source.SceneComponents.WorldTileBlockRow

---@class Source.SceneComponents.WorldTerrainOverride
---@field tileID Global.GameMap.TerrainTileID

---@class Source.SceneComponents.WorldLayerBuildState
---@field tileLayer             Engine.TileLayer
---@field layerData             Source.SceneComponents.MapLayerData
---@field layerTileset          Engine.Tileset
---@field rawAutoTiles          Source.SceneComponents.SerializedAutoTileRow[] | nil
---@field layerTerrainOverrides table<integer, table<integer, Source.SceneComponents.WorldTerrainOverride>>
---@field autoTileIndexByKey    table<string, integer>
---@field autoTilePoolSize      integer
---@field tileBlock             Source.SceneComponents.WorldTileBlock
---@field autoTileBlock         Source.SceneComponents.WorldTileBlock
---@field dataChunksByKey       table<string, Global.WorldGeometry.CellRect>
---@field writtenDataChunks     table<string, boolean>
---@field width                 integer
---@field height                integer

---@class Source.SceneComponents.WorldMapData
---@field type         "worldMap"
---@field worldName    string
---@field manifestPath string
---@field dataRoot     string
---@field width        integer
---@field height       integer
---@field layerOrder   string[]
---@field regions      Source.SceneComponents.WorldRegionData[]
---@field fog          string
---@field fogPower     integer
---@field fogOx        number
---@field fogOy        number
---@field fogDistort   integer
---@field panorama     string

---@class Source.SceneComponents.SingleFloorMapPreview
---@field gameMap GameMap
---@field mapData Source.SceneComponents.MapData

---@class Source.SceneComponents.WorldFloorMapPreview
---@field mapData Source.SceneComponents.WorldMapData
---@field regions table<string, GameMap>

---@alias Source.SceneComponents.FloorMapPreview Source.SceneComponents.SingleFloorMapPreview | Source.SceneComponents.WorldFloorMapPreview

---@brief Build map runtime objects and floor-map previews for SceneMap.
---@class Source.SceneComponents.SceneMapBuilder
---@field _floorMapPreviewGameMaps table<string, Source.SceneComponents.FloorMapPreview>
local SceneMapBuilder = {}

---@return Source.SceneComponents.SceneMapBuilder
function SceneMapBuilder.new(...) end

function SceneMapBuilder:init() end

function SceneMapBuilder:clearFloorMapPreviewCache() end

---@param data integer[] | nil
---@return sf.Color
function SceneMapBuilder.BuildAmbientLight(data) end

---@brief Resolve a map key or file path to an existing map data file.
---
--- - @param mapPath Map key or map file path.
--- - @param currentMap Current map path used to inherit extension.
--- - @return Resolved map file path relative to ``Data/Maps``.
---@param mapPath    string
---@param currentMap string | nil
---@return string
function SceneMapBuilder:resolveMapPath(mapPath, currentMap) end

---@brief Load map data from JSON format.
---
--- - @param mapPath Map key or map file path.
--- - @param currentMap Current map path used to inherit extension.
--- - @return Resolved map path and loaded map data.
---@param mapPath    string
---@param currentMap string | nil
---@return string, Source.SceneComponents.MapData | Source.SceneComponents.WorldMapData
function SceneMapBuilder:loadMapData(mapPath, currentMap) end

---@param mapPath    string
---@param currentMap string | nil
---@param position   sf.Vector2i | nil
---@return string, sf.Vector2i | nil, boolean, Source.SceneComponents.WorldRegionData | nil
function SceneMapBuilder:resolveMapDestination(mapPath, currentMap, position) end

---@brief Convert a map path to an on-disk path under ``Data/Maps``.
---
--- - @param mapPath Map file path relative to ``Data/Maps``.
--- - @return On-disk map data path.
---@param mapPath string
---@return string
function SceneMapBuilder.GetMapDataPath(mapPath) end

---@brief Generate a tilemap from map layer data.
---
--- - @param data Map layer data.
--- - @param layerOrder Ordered layer names.
--- - @param width Map width in tiles.
--- - @param height Map height in tiles.
--- - @return Generated tilemap.
---@param data       table<string, Source.SceneComponents.MapLayerData>
---@param layerOrder string[]
---@param width      integer
---@param height     integer
---@return Engine.Tilemap
function SceneMapBuilder.GenerateTilemap(data, layerOrder, width, height) end

---@param data Source.SceneComponents.MapData
---@return table<string, Engine.Actor[]>
function SceneMapBuilder.GenerateActors(data) end

---@brief Generate a game map from serialised map data.
---
--- - @param data Map data.
--- - @param camera Optional camera.
--- - @param emitCreateEvents Whether actor/component create events should run.
--- - @param previewOnly Whether to skip resources only needed by the live map.
--- - @return Generated game map.
---@param data              Source.SceneComponents.MapData
---@param camera            GlobalCore.Camera | nil
---@param emitCreateEvents? boolean
---@param previewOnly       boolean | nil
---@return GameMap
function SceneMapBuilder:generateGameMap(data, camera, emitCreateEvents, previewOnly) end

---@param worldData    Source.SceneComponents.WorldMapData
---@param region       Source.SceneComponents.WorldRegionData
---@param data         Source.SceneComponents.SerializedMapData
---@param inst         Source.GameInstance.GameInstance
---@param worldPath    string
---@param addedActors  Source.GameInstance.AddedActorRecord[]
---@param movedActors  Source.GameInstance.WorldMovedActorRecord[]
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGameMap.RegionBuildState
function SceneMapBuilder:createWorldRegionBuildState(
    worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
) end

---@param worldPath       string
---@param worldData       Source.SceneComponents.WorldMapData
---@param inst            Source.GameInstance.GameInstance
---@param initialPosition sf.Vector2i | nil
---@return Global.WorldGameMap.WorldGameMap
function SceneMapBuilder:generateWorldGameMap(worldPath, worldData, inst, initialPosition) end

---@param gameMap          GameMap
---@param addedActors      Source.GameInstance.AddedActorRecord[]
---@param emitCreateEvents boolean | nil
function SceneMapBuilder:applyAddedActors(gameMap, addedActors, emitCreateEvents) end

---@brief Build a floor teleporter preview texture.
---
--- - @param inst Current game instance.
--- - @param currentMap Current map path used to resolve extension-less map keys.
--- - @param mapKey Region map key.
--- - @param telepoint Telepoint tile position, in world coordinates for a world manifest.
--- - @param previewSize Preview texture size in pixels.
--- - @param previewScale Preview map scale.
--- - @param showTelepointMarker Whether to draw the selected telepoint marker.
--- - @return Preview texture. Loading and rendering errors are propagated.
---@param inst                Source.GameInstance.GameInstance
---@param currentMap          string | nil
---@param mapKey              string
---@param telepoint           sf.Vector2u
---@param previewSize         integer
---@param previewScale        number
---@param showTelepointMarker boolean
---@return sf.Texture
function SceneMapBuilder:buildFloorMapPreview(
    inst, currentMap, mapKey, telepoint, previewSize, previewScale, showTelepointMarker
) end

---@brief Resolve a region map key to a map data path.
---
--- - @param mapKey Region map key or map file path.
--- - @param currentMap Current map path used to inherit extension.
--- - @return Resolved map file path.
---@param mapKey     string
---@param currentMap string | nil
---@return string
function SceneMapBuilder:resolveRegionMapPath(mapKey, currentMap) end

---@param records      Source.GameInstance.WorldMovedActorRecord[]
---@param targetRegion Source.SceneComponents.WorldRegionData | nil
---@return Source.GameInstance.WorldMovedActorRecord[]
function SceneMapBuilder:selectWorldMovedActors(records, targetRegion) end

---@param actor           Engine.Actor
---@param destroyedActors table<string, boolean>
---@return boolean
function SceneMapBuilder:pruneDestroyedActorTree(actor, destroyedActors) end

---@param actorRecord          Source.GameInstance.AddedActorRecord | Source.GameInstance.WorldMovedActorRecord
---@param actorPositions       table<string, sf.Vector2i>
---@param destroyedActors      table<string, boolean>
---@param preserveRootPosition boolean
---@return Engine.Actor | nil
function SceneMapBuilder:generatePersistedActor(actorRecord, actorPositions, destroyedActors, preserveRootPosition) end

---@param actorsByTag table<string, Engine.Actor>
---@param root        Engine.Actor
function SceneMapBuilder:indexActorTreeByTag(actorsByTag, root) end

---@param region       Source.SceneComponents.WorldRegionData
---@param width        integer
---@param height       integer
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGeometry.CellRect[], Global.WorldGeometry.CellRect[], integer
function SceneMapBuilder:createWorldRegionChunks(region, width, height, priorityRect) end

---@param region       Source.SceneComponents.WorldRegionData
---@param width        integer
---@param height       integer
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGeometry.CellRect[], integer
function SceneMapBuilder:createWorldTileGraphicsChunks(region, width, height, priorityRect) end

---@param data                Source.SceneComponents.SerializedMapData
---@param terrainDestructions table<string, table<string, Source.GameInstance.TerrainChangeRecord>>
---@param yieldStep           fun()
---@return table<string, table<integer, table<integer, Source.SceneComponents.WorldTerrainOverride>>>
---@async
function SceneMapBuilder:createWorldTerrainOverrides(data, terrainDestructions, yieldStep) end

---@param actorData Source.Data.SerializedActorData
---@return Source.Data.ActorData
function SceneMapBuilder:normaliseActorData(actorData) end

---@param data Source.SceneComponents.SerializedMapData
function SceneMapBuilder:validateIncrementalMapData(data) end

---@param layerState Source.SceneComponents.WorldLayerBuildState
---@param chunk      Global.WorldGeometry.CellRect
function SceneMapBuilder:writeWorldLayerDataChunk(layerState, chunk) end

---@param layerState Source.SceneComponents.WorldLayerBuildState
---@param chunk      Global.WorldGeometry.CellRect
---@param deadline   number
---@return boolean
function SceneMapBuilder:prepareWorldLayerNativeChunk(layerState, chunk, deadline) end

return SceneMapBuilder
