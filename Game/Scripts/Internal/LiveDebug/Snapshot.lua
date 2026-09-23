local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local cjson = require("cjson")

local Snapshot = {}

---@type table<Engine.Actor, string>
local actorIds = setmetatable({}, { __mode = "k" })
---@type table<string, Engine.Actor>
local actorsById = setmetatable({}, { __mode = "v" })
local nextActorId = 0

---@param actor Engine.Actor
---@return string
local function actorId(actor)
    local result = actorIds[actor]
    if result == nil then
        nextActorId = nextActorId + 1
        result = tostring(nextActorId)
        actorIds[actor] = result
        actorsById[result] = actor
    end
    return result
end

---@param actor Engine.Actor
---@return string
local function classPath(actor)
    local actorType = Class.type(actor)
    local blueprint = rawget(actorType, "__blueprintClassPath")
    if Class.isInstance(blueprint, "string") then
        return blueprint
    end
    local moduleName = Engine.getClassModulePath(actorType)
    if moduleName == "Engine" then
        for name, value in pairs(Engine) do
            if value == actorType then
                return "Engine." .. name
            end
        end
    end
    if Class.isInstance(moduleName, "string") then
        return moduleName .. "." .. (moduleName:match("([^%.]+)$") or "Actor")
    end
    return "Engine.Actor"
end

---@param value sf.Vector2f
---@return number[]
local function vector(value)
    return { value.x, value.y }
end

---@param actor Engine.Actor
---@return Internal.LiveDebug.Visual
local function visual(actor)
    local rectangle = actor:getTextureRect()
    local texture = actor:getTexture()
    local position = actor:getPosition()
    local cellPosition = actor:getMapPosition()
    local translation = actor:getTranslation()
    local cellSize = Engine.GetCellSize()
    return {
        texturePath = texture ~= nil and GlobalCore.TextureManager.getPath(texture) or "",
        textureRect = { rectangle.position.x, rectangle.position.y, rectangle.size.x, rectangle.size.y },
        translation = {
            position.x - cellPosition.x * cellSize + translation.x,
            position.y - cellPosition.y * cellSize + translation.y
        },
        scale = vector(actor:getScale()),
        origin = vector(actor:getOrigin()),
        rotation = actor:getRotation():asDegrees(),
        hue = actor.hue,
        visible = actor:isVisibleInHierarchy(),
        shaderPath = actor:getShaderPath()
    }
end

function Snapshot.FindActor(id, view)
    local actor = actorsById[id]
    if actor == nil or actor:isDestroyed() or actor:getMap() ~= view.gameMap then
        return nil
    end
    local position = actor:getMapPosition()
    if view.region ~= nil then
        if position.x < view.x or position.y < view.y
            or position.x >= view.x + view.width or position.y >= view.y + view.height then
            return nil
        end
    end
    return actor
end

function Snapshot.Actors(view)
    local result = {}
    for _, layer in ipairs(view.tilemap:getLayerNameList()) do
        result[layer] = list():toTable()
    end
    local seen = {}
    ---@param actor           Engine.Actor | nil
    ---@param inheritedLayer? string
    local function append(actor, inheritedLayer)
        if actor == nil or seen[actor] or actor:isDestroyed() or actor:getMap() ~= view.gameMap then
            return
        end
        seen[actor] = true
        local position = actor:getMapPosition()
        local layer = view.gameMap:getActorLayer(actor) or inheritedLayer
        if layer == nil then
            layer = view.tilemap:getLayerNameList()[1]
        end
        if layer ~= nil
            and (view.region == nil
                or position.x >= view.x and position.y >= view.y
                    and position.x < view.x + view.width and position.y < view.y + view.height) then
            result[layer] = result[layer] or list():toTable()
            local path = classPath(actor)
            local parent = actor:getParent()
            result[layer][#result[layer] + 1] = {
                bp = path,
                type = path,
                tag = actor:getMapTag() or "",
                runtimeId = actorId(actor),
                parentRuntimeId = parent ~= nil and actorId(parent) or nil,
                position = { position.x - view.x, position.y - view.y },
                visual = visual(actor)
            }
        end
        for _, child in ipairs(actor:getChildren()) do
            append(child, layer)
        end
    end
    for _, actor in ipairs(view.gameMap:getAllActors()) do
        append(actor)
    end
    append(view.player)
    return result
end

function Snapshot.Map(view, actors)
    local layers = {}
    local order = view.tilemap:getLayerNameList()
    for _, name in ipairs(order) do
        local layer = assert(view.tilemap:getLayer(name))
        local data = layer:getData()
        local tiles = {}
        local autoTiles = {}
        for y = 1, view.height do
            tiles[y] = {}
            autoTiles[y] = {}
            local tileRow = data.tiles[y] or {}
            local autoRow = data.autoTiles[y] or {}
            for x = 1, view.width do
                local tile = tileRow[x]
                tiles[y][x] = tile == nil and cjson.null or tile
                local autoTile = autoRow[x]
                local key = nil
                if Class.isInstance(autoTile, "number") then
                    key = layer:getAutoTileKey(autoTile)
                elseif Class.isInstance(autoTile, "string") and bool(autoTile) then
                    key = autoTile
                end
                autoTiles[y][x] = key == nil and cjson.null or key
            end
        end
        layers[name] = {
            layerName = name,
            layerTileset = data.layerTilesetKey,
            visible = layer:getVisible(),
            tiles = tiles,
            autoTiles = autoTiles,
            shaderPath = data.shaderPath
        }
    end
    return {
        mapName = view.gameMap.mapName,
        width = view.width,
        height = view.height,
        layerOrder = order,
        layers = layers,
        actors = actors,
        BPClassVarChanged = {}
    }
end

return Snapshot
