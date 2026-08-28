local GlobalCore = require("GlobalCore")
local Logging = require("Global.Utils.Logging")
local Data = require("Source.Data")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Item = require("Source.Item")

local Special = GeneralEnum.Special

local DefeatSpawns = {}

---@class EnemyDefeatSpawnContext
---@field gameMap      GameMap
---@field layerName    string
---@field originalTag  string
---@field position     sf.Vector2i
---@field reservedTags table<string, boolean>

---@param enemy Source.Enemy
---@param scene Source.Scenes.SceneMap.SceneMap
---@return EnemyDefeatSpawnContext
local function createContext(enemy, scene)
    local gameMap = scene:getGameMap()
    local layerName = assert(gameMap:getActorLayer(enemy), "Defeated enemy is not on a map layer")
    local originalTag = enemy:getMapTag()
    assert(bool(originalTag), "Defeated enemy requires a non-empty map-placement tag")
    return {
        gameMap = gameMap,
        layerName = layerName,
        originalTag = originalTag,
        position = copy(enemy:getMapPosition()),
        reservedTags = {}
    }
end

---@param context EnemyDefeatSpawnContext
---@param suffix  string
---@return string
local function reserveTag(context, suffix)
    local baseTag = context.originalTag .. "_" .. suffix
    local mapTag = baseTag
    local tagSuffix = 2
    while context.gameMap:getActorByTag(mapTag) ~= nil or context.reservedTags[mapTag] do
        mapTag = baseTag .. "_" .. tostring(tagSuffix)
        tagSuffix = tagSuffix + 1
    end
    context.reservedTags[mapTag] = true
    return mapTag
end

---@param context       EnemyDefeatSpawnContext
---@param blueprintPath string
---@param kind          string
---@param tagSuffix     string
---@return Engine.Actor
local function prepareActor(context, blueprintPath, kind, tagSuffix)
    assert(
        type(blueprintPath) == "string" and bool(blueprintPath), "Enemy " .. kind .. " requires a Blueprint class path"
    )
    local actor = assert(
        Data.GenActorFromClassPath(blueprintPath), "Enemy " .. kind .. " Blueprint class not found: " .. blueprintPath
    )
    actor:setMapTag(reserveTag(context, tagSuffix))
    actor:setMapPosition(copy(context.position))
    return actor
end

---@param blueprintPath string
---@param position      sf.Vector2i
---@return string
local function createDropMapTag(blueprintPath, position)
    local prefix = blueprintPath:gsub("^Data%.Blueprints%.", ""):gsub("%.", "_")
    return prefix .. "_default_" .. tostring(position.x) .. "_" .. tostring(position.y)
end

---@param context       EnemyDefeatSpawnContext
---@param blueprintPath string
---@param offset        sf.Vector2i
---@return Source.Item | nil
local function prepareDrop(context, blueprintPath, offset)
    assert(type(blueprintPath) == "string" and bool(blueprintPath), "Enemy drop requires a Blueprint class path")
    local resolvedPath = Data.ResolveClassPath(blueprintPath)
    local itemClass = assert(Data.GetClass(resolvedPath), "Enemy drop Blueprint class not found: " .. resolvedPath)
    assert(Class.isSubclass(itemClass, Item), "Enemy drop Blueprint must derive from Source.Item: " .. resolvedPath)
    local position = context.position + offset
    local mapTag = createDropMapTag(resolvedPath, position)
    if context.gameMap:getActorByTag(mapTag) ~= nil or context.reservedTags[mapTag] then
        Logging.warning(
            "Skipping enemy drop %s at (%d, %d): map tag already exists: %s", resolvedPath, position.x, position.y,
            mapTag
        )
        return nil
    end
    local actor = assert(
        Data.GenActorFromClassPath(resolvedPath), "Enemy drop Blueprint class not found: " .. resolvedPath
    )
    ---@cast actor Source.Item
    context.reservedTags[mapTag] = true
    actor:setMapTag(mapTag)
    actor:setMapPosition(position)
    return actor
end

function DefeatSpawns.Prepare(enemy, scene)
    ---@type string | nil
    local blueprintPath = enemy.infoComp.special[Special.Reborn]
    local drops = enemy:getDrops()
    if blueprintPath == nil and not bool(drops) then
        return nil, {}, nil
    end
    local context = createContext(enemy, scene)
    local rebornActor = nil
    if blueprintPath ~= nil then
        rebornActor = prepareActor(context, blueprintPath, "Reborn special", "Reborn")
    end
    local droppedActors = {}
    for _, dropPath in ipairs(table.orderedStringKeys(drops)) do
        local actor = prepareDrop(context, dropPath, drops[dropPath])
        if actor ~= nil then
            droppedActors[#droppedActors + 1] = actor
        end
    end
    return rebornActor, droppedActors, context.layerName
end

function DefeatSpawns.Spawn(scene, actor, layerName)
    scene:getGameMap():spawnActor(actor, layerName)
    scene:recordAddedActor(actor)
end

function DefeatSpawns.GameOver()
    local SceneGameOver = require("Source.Scenes.SceneGameOver")

    GlobalCore.System.setScene(SceneGameOver.new())
end

return DefeatSpawns
