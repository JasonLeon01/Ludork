---@meta Source.SceneComponents.MapBuilder
---@class Source.SceneComponents.MapData
---@field mapName           string
---@field width             integer
---@field height            integer
---@field layerOrder        string[]
---@field layers            table<string, table>
---@field actors            table<string, Source.Data.ActorData[]>
---@field BPClassVarChanged table<string, table<string, Source.Data.ClassVarValue>> | nil
---@field ambientLight      sf.Color
---@field lights            GlobalCore.Light[]
---@field bgm               string | nil
---@field bgs               string | nil
---@field bgmFilter         Source.SceneComponents.MusicFilterData | nil
---@field bgsFilter         Source.SceneComponents.MusicFilterData | nil
---@field fog               string | nil
---@field fogPower          number | nil
---@field fogOx             number | nil
---@field fogOy             number | nil
---@field fogDistort        number | nil

---@brief Build map runtime objects and floor-map previews for SceneMap.
---@class Source.SceneComponents.SceneMapBuilder
---@field _floorMapPreviewGameMaps table<string, { gameMap: GameMap, mapData: Source.SceneComponents.MapData }>
local SceneMapBuilder = {}

---@return Source.SceneComponents.SceneMapBuilder
function SceneMapBuilder.new(...) end

function SceneMapBuilder:init() end

function SceneMapBuilder:clearFloorMapPreviewCache() end

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
---@return string, Source.SceneComponents.MapData
function SceneMapBuilder:loadMapData(mapPath, currentMap) end

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
---@param data       table<string, table>
---@param layerOrder string[]
---@param width      integer
---@param height     integer
---@return Engine.Tilemap
function SceneMapBuilder.GenerateTilemap(data, layerOrder, width, height) end

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

---@param gameMap          GameMap
---@param addedActors      Source.GameInstance.AddedActorRecord[]
---@param emitCreateEvents boolean | nil
function SceneMapBuilder:applyAddedActors(gameMap, addedActors, emitCreateEvents) end

---@brief Build a floor teleporter preview texture.
---
--- - @param inst Current game instance.
--- - @param currentMap Current map path used to resolve extension-less map keys.
--- - @param mapKey Region map key.
--- - @param telepoint Telepoint tile position.
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

---@brief Get the teleporter actor tag for a floor telepoint.
---
--- - @param currentMap Current map path used to resolve extension-less map keys.
--- - @param mapKey Region map key.
--- - @param telepoint Telepoint tile position.
--- - @return Teleporter actor tag, or nil.
---@param currentMap string | nil
---@param mapKey     string
---@param telepoint  sf.Vector2u
---@return string | nil
function SceneMapBuilder:getFloorTelepointTag(currentMap, mapKey, telepoint) end

---@brief Resolve a region map key to a map data path.
---
--- - @param mapKey Region map key or map file path.
--- - @param currentMap Current map path used to inherit extension.
--- - @return Resolved map file path.
---@param mapKey     string
---@param currentMap string | nil
---@return string
function SceneMapBuilder:resolveRegionMapPath(mapKey, currentMap) end

return SceneMapBuilder
