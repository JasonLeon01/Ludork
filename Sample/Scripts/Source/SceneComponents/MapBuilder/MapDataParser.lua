local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local WorldGeometry = require("Global.WorldGeometry")
local MapPath = require("Source.MapPath")
local System = require("Source.System")

local WORLD_MANIFEST_FIELDS = {
    type = true,
    worldName = true,
    width = true,
    height = true,
    fog = true,
    fogPower = true,
    fogOx = true,
    fogOy = true,
    fogDistort = true,
    layerOrder = true,
    placements = true
}
local WORLD_PLACEMENT_FIELDS = { map = true, rect = true }

local MapDataParser = {}
MapDataParser.DATA_ROOT = os.path.join(".", "Data", "Maps")
MapDataParser.EXTENSION = ".json"
MapDataParser.WORLD_MANIFEST_FILE = "_world.json"

local function requireInteger(value, path, minimum)
    assert(Class.isInstance(value, "number") and math.type(value) == "integer", path .. " must be an integer")
    local integerValue = math.tointeger(value)
    ---@cast integerValue integer
    assert(integerValue >= minimum, path .. " must be at least " .. minimum)
    return integerValue
end

local function requireFiniteNumber(value, path)
    assert(
        Class.isInstance(value, "number") and value == value and value ~= math.huge and value ~= -math.huge,
        path .. " must be a finite number"
    )
    return value
end

local function rejectUnknownFields(value, allowed, path)
    for key in pairs(value) do
        assert(Class.isInstance(key, "string") and allowed[key], path .. " contains an unknown field: " .. tostring(key))
    end
end

local function requireArray(value, path)
    assert(Class.isInstance(value, "table"), path .. " must be an array")
    local length = #value
    for index = 1, length do
        assert(value[index] ~= nil, path .. " must be an array")
    end
    for key in pairs(value) do
        assert(
            Class.isInstance(key, "number") and math.type(key) == "integer" and key >= 1 and key <= length,
            path .. " must be an array"
        )
    end
end

function MapDataParser.GetDataPath(mapPath)
    return os.path.join(MapDataParser.DATA_ROOT, MapPath.Normalise(mapPath))
end

function MapDataParser.IsWorldManifest(mapPath)
    return os.path.basename(mapPath) == MapDataParser.WORLD_MANIFEST_FILE
end

function MapDataParser.GetPathCandidates(mapPath, currentMap)
    mapPath = MapPath.Normalise(mapPath)
    if not bool(mapPath) then
        return {}
    end
    local candidates = {}
    local seen = {}
    local function append(candidate)
        if not seen[candidate] then
            seen[candidate] = true
            candidates[#candidates + 1] = candidate
        end
    end
    local stem, extension = os.path.splitext(mapPath)
    extension = extension:lower()
    if bool(extension) then
        append(mapPath)
        if extension == MapDataParser.EXTENSION then
            append(stem .. MapDataParser.EXTENSION)
        end
    else
        local _, currentExtension = os.path.splitext(MapPath.Normalise(currentMap or System.GetStartMap()))
        if currentExtension:lower() == MapDataParser.EXTENSION then
            append(mapPath .. currentExtension:lower())
        end
        append(mapPath .. MapDataParser.EXTENSION)
    end
    return candidates
end

function MapDataParser.NormaliseWorld(data, manifestPath)
    assert(data.type == "worldMap", "World manifest type must be 'worldMap': " .. manifestPath)
    assert(data.bgm == nil and data.bgs == nil
            and data.bgmFilter == nil and data.bgsFilter == nil
            and data.ambientLight == nil,
        "World manifest must not define BGM, BGS, audio filters, or ambientLight: " .. manifestPath)
    rejectUnknownFields(data, WORLD_MANIFEST_FIELDS, "worldMap")
    assert(Class.isInstance(data.worldName, "string") and bool(data.worldName), "worldMap.worldName must be a non-empty string")
    data.width = requireInteger(data.width, "worldMap.width", 1)
    data.height = requireInteger(data.height, "worldMap.height", 1)
    assert(Class.isInstance(data.fog, "string"), "worldMap.fog must be a string")
    data.fogPower = requireInteger(data.fogPower, "worldMap.fogPower", 0)
    data.fogOx = requireFiniteNumber(data.fogOx, "worldMap.fogOx")
    data.fogOy = requireFiniteNumber(data.fogOy, "worldMap.fogOy")
    data.fogDistort = requireInteger(data.fogDistort, "worldMap.fogDistort", 0)
    requireArray(data.layerOrder, "worldMap.layerOrder")
    local seenLayers = {}
    for index, layerName in ipairs(data.layerOrder) do
        assert(
            Class.isInstance(layerName, "string") and bool(layerName),
            "worldMap.layerOrder[" .. index .. "] must be a non-empty string"
        )
        assert(not seenLayers[layerName], "worldMap.layerOrder contains a duplicate layer: " .. layerName)
        seenLayers[layerName] = true
    end
    local worldDirectory = os.path.dirname(manifestPath)
    local directoryName = os.path.basename(worldDirectory)
    assert(bool(directoryName), "World manifest must be inside a direct world directory")
    assert(os.path.dirname(worldDirectory) == "", "World directories cannot be nested: " .. worldDirectory)
    requireArray(data.placements, "worldMap.placements")
    local regions = {}
    local seenMaps = {}
    for index, placement in ipairs(data.placements) do
        assert(Class.isInstance(placement, "table"), "worldMap.placements[" .. index .. "] must be an object")
        rejectUnknownFields(placement, WORLD_PLACEMENT_FIELDS, "worldMap.placements[" .. index .. "]")
        local map = placement.map
        assert(
            Class.isInstance(map, "string") and bool(map), "worldMap.placements[" .. index .. "].map must be a non-empty string"
        )
        map = MapPath.Normalise(map)
        assert(os.path.dirname(map) == "", "World child map must be a direct file: " .. map)
        local _, extension = os.path.splitext(map)
        assert(
            extension:lower() == MapDataParser.EXTENSION and map ~= MapDataParser.WORLD_MANIFEST_FILE,
            "World child map must be a .json map file: " .. map
        )
        assert(not seenMaps[map], "World child map is placed more than once: " .. map)
        seenMaps[map] = true
        local rect = placement.rect
        requireArray(rect, "worldMap.placements[" .. index .. "].rect")
        assert(#rect == 4, "worldMap.placements[" .. index .. "].rect must contain x, y, width, and height")
        local x = requireInteger(rect[1], "worldMap.placements[" .. index .. "].rect[1]", 0)
        local y = requireInteger(rect[2], "worldMap.placements[" .. index .. "].rect[2]", 0)
        local width = requireInteger(rect[3], "worldMap.placements[" .. index .. "].rect[3]", 1)
        local height = requireInteger(rect[4], "worldMap.placements[" .. index .. "].rect[4]", 1)
        assert(
            x + width <= data.width and y + height <= data.height, "World child map is outside world bounds: " .. map
        )
        local path = MapPath.Normalise(os.path.join(worldDirectory, map))
        assert(Engine.jsonExists(MapDataParser.GetDataPath(path)), "World child map does not exist: " .. path)
        local region = {
            index = index,
            map = map,
            path = path,
            x = x,
            y = y,
            width = width,
            height = height,
            state = "Unloaded",
            wasActive = false
        }
        for _, existing in ipairs(regions) do
            assert(
                not WorldGeometry.RectIntersects(region, existing),
                "World child maps overlap: " .. existing.map .. " and " .. map
            )
        end
        regions[#regions + 1] = region
    end
    if not bool(regions) then
        assert(not bool(data.layerOrder), "worldMap.layerOrder must be empty when no child maps are placed")
    end
    data.manifestPath = manifestPath
    data.dataRoot = MapDataParser.GetDataPath(worldDirectory)
    data.regions = regions
end

function MapDataParser.NormaliseMap(data, buildAmbientLight)
    local seenLayers = {}
    for _, layerName in ipairs(data.layerOrder) do
        assert(data.layers[layerName] ~= nil, "Map layerOrder references a missing layer: " .. layerName)
        assert(not seenLayers[layerName], "Map layerOrder contains a duplicate layer: " .. layerName)
        seenLayers[layerName] = true
    end
    for layerName in pairs(data.layers) do
        assert(seenLayers[layerName], "Map layer is missing from layerOrder: " .. layerName)
    end
    for layerName in pairs(data.actors or {}) do
        assert(seenLayers[layerName], "Map actor group is missing from layerOrder: " .. layerName)
    end
    local lights = {}
    for index, lightData in ipairs(data.lights or {}) do
        lights[index] = GlobalCore.Light.fromDict(lightData)
    end
    local serializedActors = data.actors or {}
    ---@type table<string, Source.Data.ActorData[]>
    local actors = {}
    for layerName, actorDatas in pairs(serializedActors) do
        local normalisedActorDatas = {}
        actors[layerName] = normalisedActorDatas
        for index, actorData in ipairs(actorDatas) do
            local position = actorData.position or { 0, 0 }
            local x = position[1] or 0
            local y = position[2] or 0
            normalisedActorDatas[index] = { bp = actorData.bp, tag = actorData.tag, position = sf.Vector2u.new(x, y) }
        end
    end
    ---@type Source.SceneComponents.MapData
    local mapData = {
        type = data.type,
        mapName = data.mapName,
        width = data.width,
        height = data.height,
        layerOrder = data.layerOrder,
        layers = data.layers,
        actors = actors,
        BPClassVarChanged = data.BPClassVarChanged,
        ambientLight = buildAmbientLight(data.ambientLight),
        lights = lights,
        bgm = data.bgm,
        bgs = data.bgs,
        bgmFilter = data.bgmFilter,
        bgsFilter = data.bgsFilter,
        fog = data.fog,
        fogPower = data.fogPower,
        fogOx = data.fogOx,
        fogOy = data.fogOy,
        fogDistort = data.fogDistort
    }
    for key, value in pairs(data) do
        if rawget(mapData, key) == nil then
            rawset(mapData, key, value)
        end
    end
    return mapData
end

return MapDataParser
