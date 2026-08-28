---@meta Source.SceneComponents.MapBuilder.MapDataParser

local MapDataParser = {}

MapDataParser.DATA_ROOT = ""
MapDataParser.EXTENSION = ""
MapDataParser.WORLD_MANIFEST_FILE = ""

---@param mapPath string
---@return string
function MapDataParser.GetDataPath(mapPath) end

---@param mapPath string
---@return boolean
function MapDataParser.IsWorldManifest(mapPath) end

---@param mapPath    string
---@param currentMap string | nil
---@return string[]
function MapDataParser.GetPathCandidates(mapPath, currentMap) end

---@param data         table
---@param manifestPath string
function MapDataParser.NormaliseWorld(data, manifestPath) end

---@param data              Source.SceneComponents.SerializedMapData
---@param buildAmbientLight fun(data: integer[] | nil): sf.Color
---@return Source.SceneComponents.MapData
function MapDataParser.NormaliseMap(data, buildAmbientLight) end

return MapDataParser
