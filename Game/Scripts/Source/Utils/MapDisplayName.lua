local Engine = require("Engine")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local MapConstants = require("Source.Configs.MapConstants")
local LocaleCore = require("Source.Locale.Core")

local MapDisplayName = {}

function MapDisplayName.Resolve(mapKey, currentMapPath)
    local builder = SceneMapBuilder.new()
    local mapPath = builder:resolveMapPath(mapKey, currentMapPath)
    if not Engine.dataExists(SceneMapBuilder.GetMapDataPath(mapPath)) then
        return LocaleCore.ApplyStringLocaleFormat(mapKey)
    end
    local _, mapData = builder:loadMapData(mapPath, currentMapPath)
    local mapName = mapData.type == MapConstants.WORLD_MAP_TYPE and mapData.worldName or mapData.mapName
    return LocaleCore.ApplyStringLocaleFormat(bool(mapName) and tostring(mapName) or mapKey)
end

return MapDisplayName
