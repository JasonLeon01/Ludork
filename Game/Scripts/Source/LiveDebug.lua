local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local Snapshot = require("Source.LiveDebug.Snapshot")
local TileUpdates = require("Source.LiveDebug.Tiles")
local Values = require("Source.LiveDebug.Values")
local MapClickAutoPath = require("Source.SceneComponents.MapClickAutoPath")
local cjson = require("cjson")

local LiveDebug = {}

---@type table<integer, Source.Scenes.SceneMap.SceneMap>
local sceneRef = setmetatable({}, { __mode = "v" })
---@type table<integer, GameMap>
local mapRef = setmetatable({}, { __mode = "v" })
local installed = false
local enabled = false
local contextCounter = 0
local context = "0"
local currentMapKey = ""
local currentRegionKey = ""
local currentStatus = "no_scene"

---@return Source.LiveDebug.View | nil, string, GameMap | nil, string
local function resolveView()
    local scene = sceneRef[1]
    if scene == nil or GlobalCore.SceneManager.getScene() ~= scene then
        return nil, "no_scene", nil, ""
    end
    local player = scene:getGameInstance():getPlayer()
    if player == nil or player:isDestroyed() then
        return nil, "no_player", nil, ""
    end
    local gameMap = player:getMap()
    if gameMap == nil then
        return nil, "no_map", nil, ""
    end
    ---@cast gameMap GameMap
    local region = nil
    local mapKey = scene:getGameInstance():getCurrentMapPath() or ""
    local tilemap = gameMap:getTilemap()
    local size = gameMap:getSize()
    local x, y, width, height = 0, 0, size.x, size.y
    if gameMap:isWorldMap() then
        ---@cast gameMap Global.WorldGameMap.WorldGameMap
        region = gameMap:getRegionPosition(player:getMapPosition())
        if region == nil then
            return nil, "world_hole", gameMap, ""
        end
        mapKey = region.path
        local builder = region.backgroundBuilder
        if region.payload == nil or region.publishState ~= nil or builder ~= nil and not builder.completed then
            return nil, "loading_map", gameMap, mapKey
        end
        tilemap = region.payload.terrain:getTilemap()
        x, y, width, height = region.x, region.y, region.width, region.height
    end
    return {
        gameMap = gameMap, tilemap = tilemap, player = player, region = region,
        x = x, y = y, width = width, height = height, mapKey = mapKey:gsub("%.json$", "")
    },
        "ready",
        gameMap,
        mapKey
end

---@return Source.LiveDebug.View | nil
local function currentView()
    local view, status, gameMap, regionKey = resolveView()
    ---@type Global.GameMap.TerrainChanges.Owner | nil
    local owner = view ~= nil and view.gameMap or nil
    if view ~= nil and view.region ~= nil then
        owner = assert(view.region.payload).terrain
    end
    if gameMap ~= mapRef[1] or regionKey ~= currentRegionKey
        or status ~= currentStatus or enabled and owner ~= TileUpdates.GetOwner() then
        contextCounter = contextCounter + 1
        context = tostring(contextCounter)
        mapRef[1] = gameMap
        currentRegionKey = regionKey
        currentStatus = status
        currentMapKey = regionKey:gsub("%.json$", "")
        TileUpdates.Reset()
    end
    if enabled and owner ~= nil then
        TileUpdates.Bind(owner)
    end
    return view
end

---@param view Source.LiveDebug.View | nil
---@return Source.LiveDebug.Response
local function responseBase(view)
    return { success = true, context = context, editable = view ~= nil, mapKey = currentMapKey, status = currentStatus }
end

---@param request  Source.LiveDebug.Request
---@param view     Source.LiveDebug.View | nil
---@param forceMap boolean
---@return Source.LiveDebug.Response
local function poll(request, view, forceMap)
    local response = responseBase(view)
    if view == nil then
        return response
    end
    response.actors = Snapshot.Actors(view)
    local fullMap, tileChanges = TileUpdates.Collect(view, forceMap or request.context ~= context)
    if fullMap then
        response.map = Snapshot.Map(view, response.actors)
    end
    response.tileChanges = tileChanges
    response.tileUpdated = fullMap or tileChanges ~= nil
    if request.includeInfo == true and Class.isInstance(request.actorId, "string") then
        local id = request.actorId
        ---@cast id string
        local actor = Snapshot.FindActor(id, view)
        if actor ~= nil then
            response.info = Values.Read(actor, id)
            response.variables = { [id] = response.info.values }
        end
    end
    return response
end

---@param view Source.LiveDebug.View
---@param x    integer | nil
---@param y    integer | nil
---@return sf.Vector2i
local function requirePosition(view, x, y)
    assert(Class.isInstance(x, "number"), "Tile x must be an integer")
    assert(Class.isInstance(y, "number"), "Tile y must be an integer")
    local column = assert(math.tointeger(assert(x)), "Tile x must be an integer")
    local row = assert(math.tointeger(assert(y)), "Tile y must be an integer")
    assert(
        column >= 0 and row >= 0 and column < view.width and row < view.height, "Position is outside the current map"
    )
    local position = sf.Vector2i.new(column + view.x, row + view.y)
    ---@cast position sf.Vector2i
    return position
end

---@param actor    Engine.Actor
---@param view     Source.LiveDebug.View
---@param position sf.Vector2i
local function moveActor(actor, view, position)
    if actor == view.player then
        MapClickAutoPath.CancelForMap(view.gameMap)
    end
    actor:setRoute(nil)
    actor:stop()
    actor:setMapPosition(position)
    view.gameMap:markPassabilityDirty()
end

---@param request Source.LiveDebug.Request
---@return Source.LiveDebug.Response
local function handle(request)
    if request.action == "start" then
        enabled = true
        return poll(request, currentView(), true)
    end
    local view = currentView()
    local response = responseBase(view)
    if not enabled then
        response.success = false
        response.error = "not_started"
        return response
    end
    if request.action == "poll" then
        return poll(request, view, false)
    end
    if request.context ~= context then
        response.success = false
        response.error = "stale_context"
        return response
    end
    if view == nil then
        response.success = false
        response.error = currentStatus
        return response
    end
    if request.action == "tiles" then
        local layerName = assert(request.layer, "Layer name is required")
        assert(Class.isInstance(layerName, "string"), "Layer name must be a string")
        local layer = assert(view.tilemap:getLayer(layerName), "Layer is not present on the current map")
        assert(layer:getVisible(), "The selected tile layer is hidden")
        local tileCount = #layer:getData().layerTileset.materials
        local tiles = assert(request.tiles, "Tiles are required")
        assert(Class.isInstance(tiles, "table"), "Tiles must be an array")
        assert(#tiles <= 128, "A tile request cannot contain more than 128 cells")
        local cells = {}
        for _, tile in ipairs(tiles) do
            assert(Class.isInstance(tile, "table"), "Each tile must be an object")
            local position = requirePosition(view, tile.x, tile.y)
            local id = tile.tileId
            assert(
                id == nil or id == cjson.null or Class.isInstance(id, "string")
                    or Class.isInstance(id, "number") and math.tointeger(id) ~= nil, "Invalid tile ID"
            )
            if Class.isInstance(id, "string") and bool(id) then
                ---@cast id string
                assert(Data.HasAutoTile(id), "Unknown autotile: " .. id)
            elseif Class.isInstance(id, "number") then
                assert(id >= 0 and id < tileCount, "Tile ID is outside the layer tileset")
            end
            if id == cjson.null or id == "" then
                id = nil
            elseif Class.isInstance(id, "number") then
                id = math.tointeger(id)
            end
            ---@cast id integer | string | nil
            cells[tostring(tile.x) .. ":" .. tostring(tile.y)] = { position = position, tileId = id }
        end
        local groups = {}
        for _, cell in pairs(cells) do
            local id = cell.tileId
            if view.gameMap:getTerrainTile(layerName, cell.position) ~= id then
                local key = id == nil and "empty" or Class.isInstance(id, "string") and "auto:" .. id or "tile:" .. id
                local group = groups[key]
                if group == nil then
                    group = { tileId = id, positions = {} }
                    groups[key] = group
                end
                group.positions[#group.positions + 1] = cell.position
            end
        end
        for _, group in pairs(groups) do
            view.gameMap:setTerrainTiles(layerName, group.positions, group.tileId)
        end
        return poll(request, currentView(), false)
    end
    local id = assert(request.actorId, "Actor ID is required")
    assert(Class.isInstance(id, "string"), "Actor ID must be a string")
    local actor = Snapshot.FindActor(id, view)
    if actor == nil then
        response.success = false
        response.error = "actor_missing"
        return response
    end
    if request.action == "info" then
        response.info = Values.Read(actor, id)
        response.variables = { [id] = response.info.values }
        return response
    elseif request.action == "move" then
        if actor:getParent() ~= nil then
            response.success = false
            response.error = "child_actor_move_forbidden"
            return response
        end
        moveActor(actor, view, requirePosition(view, request.x, request.y))
    elseif request.action == "delete" then
        if actor == view.player then
            MapClickAutoPath.CancelForMap(view.gameMap)
        end
        actor:setRoute(nil)
        actor:stop()
        actor:destroy()
    elseif request.action == "setVariable" then
        Values.Write(actor, assert(request.name, "Variable name is required"), request.value)
    else
        response.success = false
        response.error = "unknown_action"
        return response
    end
    local updatedView = currentView()
    response = poll(request, updatedView, false)
    if request.action == "setVariable" and updatedView ~= nil then
        response.info = Values.Read(actor, id)
        response.variables = { [id] = response.info.values }
    end
    return response
end

function LiveDebug.Install()
    if installed or os.getenv("LUDORK_EDITOR") ~= "1" or os.getenv("LUDORK_LIVE_DEBUG") ~= "1" then
        return
    end
    Engine.EditorLiveDebug.install(handle)
    installed = true
end

function LiveDebug.Uninstall()
    if installed then
        Engine.EditorLiveDebug.uninstall()
        installed = false
    end
    enabled = false
    sceneRef[1] = nil
    mapRef[1] = nil
    TileUpdates.Reset()
end

function LiveDebug.BindScene(scene)
    sceneRef[1] = scene
end

function LiveDebug.UnbindScene(scene)
    if sceneRef[1] == scene then
        sceneRef[1] = nil
        TileUpdates.Reset()
    end
end

return LiveDebug
