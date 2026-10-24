---@meta Source.SceneComponents.MapBuilder.MapDataParser

local MapDataParser = {}

MapDataParser.DATA_ROOT = ""

---@param mapPath string
---@return string
function MapDataParser.GetDataPath(mapPath) end

---@param mapPath string
---@return boolean
function MapDataParser.IsWorldManifest(mapPath) end

---@param data Global.GameMap.SerializedMapFogSettings
---@return Engine.MapFogSettings
function MapDataParser.BuildFog(data) end

---@param mapPath    string
---@param currentMap string | nil
---@return string[]
function MapDataParser.GetPathCandidates(mapPath, currentMap) end

---@param data         table
---@param manifestPath string
function MapDataParser.NormaliseWorld(data, manifestPath) end

--- Validate layer order, layer membership and Actor group membership for ordinary and streamed maps.
---@param data Global.GameMap.SerializedMapData
function MapDataParser.ValidateMapData(data) end

--- Convert an Actor placement to native unsigned cell coordinates, defaulting missing coordinates to zero.
---@param actorData Global.Data.SerializedActorData
---@return Global.Data.ActorData
function MapDataParser.NormaliseActorData(actorData) end

---@param data              Global.GameMap.SerializedMapData
---@param buildAmbientLight fun(data: integer[] | nil): sf.Color
---@return Global.GameMap.MapData
function MapDataParser.NormaliseMap(data, buildAmbientLight) end

return MapDataParser
