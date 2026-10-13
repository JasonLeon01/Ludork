local Engine = require("Engine")
local WorldGeometry = require("Global.WorldGeometry")
local MapPath = require("Source.Utils.MapPath")
local System = require("Source.System")
local MapConstants = require("Source.Configs.MapConstants")

local ResourceFileConstants = Engine.ResourceFileConstants

local WORLD_MANIFEST_FIELDS = {
    type = true,
    worldName = true,
    width = true,
    height = true,
    fog = true,
    panorama = true,
    layerOrder = true,
    placements = true
}
local WORLD_PLACEMENT_FIELDS = { map = true, rect = true }
local FOG_FIELDS = {
    enabled = true,
    colour = true,
    density = true,
    scale = true,
    velocity = true,
    turbulence = true,
    seed = true
}

local MapDataParser = {}
MapDataParser.DATA_ROOT = os.path.join(".", "Data", "Maps")

local function requireInteger(value, path, minimum)
    assert(Class.isInstance(value, "number") and math.type(value) == "integer", path .. " must be an integer")
    local integerValue = math.tointeger(value)
    ---@cast integerValue integer
    assert(integerValue >= minimum, path .. " must be at least " .. minimum)
    return integerValue
end

local function requireFiniteNumber(value, path)
    assert(math.isFinite(value), path .. " must be a finite number")
    return value
end

local function rejectUnknownFields(value, allowed, path)
    for key in pairs(value) do
        assert(
            Class.isInstance(key, "string") and allowed[key], path .. " contains an unknown field: " .. tostring(key)
        )
    end
end

local function requireArray(value, path)
    assert(Class.isInstance(value, "table"), path .. " must be an array")
    local length = #value
    assert(value.n == nil or value.n == length, path .. " must be a dense array")
    for index = 1, length do
        assert(value[index] ~= nil, path .. " must be an array")
    end
    for key in pairs(value) do
        assert(
            key == "n" or (Class.isInstance(key, "number") and math.type(key) == "integer" and key >= 1
                    and key <= length), path .. " must be an array"
        )
    end
end

function MapDataParser.GetDataPath(mapPath)
    return os.path.join(MapDataParser.DATA_ROOT, MapPath.Normalise(mapPath))
end

function MapDataParser.IsWorldManifest(mapPath)
    return os.path.basename(mapPath) == MapConstants.WORLD_MANIFEST_FILE
end

function MapDataParser.BuildFog(data)
    assert(Class.isInstance(data, "table"), "fog must be an object")
    rejectUnknownFields(data, FOG_FIELDS, "fog")
    assert(Class.isInstance(data.enabled, "boolean"), "fog.enabled must be a boolean")
    requireArray(data.colour, "fog.colour")
    assert(#data.colour == 4, "fog.colour must contain four colour channels")
    for index, channel in ipairs(data.colour) do
        local value = requireInteger(channel, "fog.colour[" .. index .. "]", 0)
        assert(value <= 255, "fog.colour channels must be at most 255")
    end
    local density = requireFiniteNumber(data.density, "fog.density")
    local scale = requireFiniteNumber(data.scale, "fog.scale")
    local turbulence = requireFiniteNumber(data.turbulence, "fog.turbulence")
    assert(density >= 0 and density <= 1, "fog.density must be from 0 to 1")
    assert(scale > 0, "fog.scale must be positive")
    assert(turbulence >= 0 and turbulence <= 1, "fog.turbulence must be from 0 to 1")
    requireArray(data.velocity, "fog.velocity")
    assert(#data.velocity == 2, "fog.velocity must contain two numbers")
    local velocityX = requireFiniteNumber(data.velocity[1], "fog.velocity[1]")
    local velocityY = requireFiniteNumber(data.velocity[2], "fog.velocity[2]")
    local seed = requireInteger(data.seed, "fog.seed", -2147483648)
    assert(seed <= 2147483647, "fog.seed must fit a signed 32-bit integer")
    return Engine.MapFogSettings.new({
        enabled = data.enabled,
        colour = sf.Color.new(data.colour[1], data.colour[2], data.colour[3], data.colour[4]),
        density = density,
        scale = scale,
        velocity = sf.Vector2f.new(velocityX, velocityY),
        turbulence = turbulence,
        seed = seed
    })
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
        if extension == ResourceFileConstants.DATA_EXTENSION then
            append(stem .. ResourceFileConstants.DATA_EXTENSION)
        end
    else
        local _, currentExtension = os.path.splitext(MapPath.Normalise(currentMap or System.GetStartMap()))
        if currentExtension:lower() == ResourceFileConstants.DATA_EXTENSION then
            append(mapPath .. currentExtension:lower())
        end
        append(mapPath .. ResourceFileConstants.DATA_EXTENSION)
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
    assert(
        Class.isInstance(data.worldName, "string") and bool(data.worldName),
        "worldMap.worldName must be a non-empty string"
    )
    data.width = requireInteger(data.width, "worldMap.width", 1)
    data.height = requireInteger(data.height, "worldMap.height", 1)
    data.fog = MapDataParser.BuildFog(data.fog)
    if data.panorama == nil then
        data.panorama = ""
    else
        assert(Class.isInstance(data.panorama, "string"), "worldMap.panorama must be a string")
    end
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
            Class.isInstance(map, "string") and bool(map),
            "worldMap.placements[" .. index .. "].map must be a non-empty string"
        )
        map = MapPath.Normalise(map)
        assert(os.path.dirname(map) == "", "World child map must be a direct file: " .. map)
        local _, extension = os.path.splitext(map)
        assert(
            extension:lower() == ResourceFileConstants.DATA_EXTENSION and map ~= MapConstants.WORLD_MANIFEST_FILE,
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
        assert(Engine.dataExists(MapDataParser.GetDataPath(path)), "World child map does not exist: " .. path)
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
        assert(#data.layerOrder == 0, "worldMap.layerOrder must be empty when no child maps are placed")
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
        bgm = data.bgm,
        bgs = data.bgs,
        bgmFilter = data.bgmFilter,
        bgsFilter = data.bgsFilter,
        fog = MapDataParser.BuildFog(data.fog),
        panorama = data.panorama
    }
    for key, value in pairs(data) do
        if rawget(mapData, key) == nil then
            rawset(mapData, key, value)
        end
    end
    return mapData
end

return MapDataParser
