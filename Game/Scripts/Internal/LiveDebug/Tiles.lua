local TerrainChanges = require("Global.GameMap.TerrainChanges")
local WorldGeometry = require("Global.WorldGeometry")
local cjson = require("cjson")

local Tiles = {}

local MAX_PENDING_CELLS = 4096
---@type table<integer, Global.GameMap.TerrainChanges.Owner>
local ownerRef = setmetatable({}, { __mode = "v" })
---@type table<string, Engine.TileLayer>
local layerRefs = {}
---@type table<string, Engine.TileLayer>
local publishedLayerRefs = {}
---@type table<string, table<string, Internal.LiveDebug.TileChange>>
local pendingCells = {}
local pendingCellCount = 0
local needsFullMap = true
local previousLayerNames = {}
local previousLayerVisibility = {}
local previousWidth = 0
local previousHeight = 0

---@param layerName     string
---@param positions     sf.Vector2i[]
---@param tileId        Global.GameMap.TerrainTileID
---@param previousLayer Engine.TileLayer
---@param currentLayer  Engine.TileLayer
local function onTerrainChanged(layerName, positions, tileId, previousLayer, currentLayer)
    if previousLayer ~= (publishedLayerRefs[layerName] or layerRefs[layerName]) then
        needsFullMap = true
    end
    publishedLayerRefs[layerName] = currentLayer
    if needsFullMap then
        return
    end
    ---@type integer | lightuserdata
    local tile = cjson.null
    ---@type string | lightuserdata
    local autoTile = cjson.null
    if Class.isInstance(tileId, "string") and bool(tileId) then
        ---@cast tileId string
        autoTile = tileId
    elseif Class.isInstance(tileId, "number") then
        ---@cast tileId integer
        tile = tileId
    end
    local cells = pendingCells[layerName]
    if cells == nil then
        cells = {}
        pendingCells[layerName] = cells
    end
    for _, position in ipairs(positions) do
        local key = WorldGeometry.GridKey(position.x, position.y)
        if cells[key] == nil then
            pendingCellCount = pendingCellCount + 1
        end
        if pendingCellCount > MAX_PENDING_CELLS then
            needsFullMap = true
            pendingCells = {}
            pendingCellCount = 0
            return
        end
        cells[key] = { layer = layerName, x = position.x, y = position.y, tile = tile, autoTile = autoTile }
    end
end

function Tiles.GetOwner()
    return ownerRef[1]
end

function Tiles.Reset()
    local owner = ownerRef[1]
    if owner ~= nil then
        TerrainChanges.Unsubscribe(owner)
    end
    ownerRef[1] = nil
    layerRefs = {}
    publishedLayerRefs = {}
    pendingCells = {}
    pendingCellCount = 0
    previousLayerNames = {}
    previousLayerVisibility = {}
    previousWidth = 0
    previousHeight = 0
    needsFullMap = true
end

function Tiles.Bind(owner)
    if ownerRef[1] ~= owner then
        Tiles.Reset()
        ownerRef[1] = owner
        TerrainChanges.Subscribe(owner, onTerrainChanged)
    end
end

function Tiles.Collect(view, forceFullMap)
    local names = view.tilemap:getLayerNameList()
    local fullMap = forceFullMap or needsFullMap or previousWidth ~= view.width or previousHeight ~= view.height
        or #names ~= #previousLayerNames
    local currentLayerRefs = {}
    local currentLayerVisibility = {}
    for index, name in ipairs(names) do
        local layer = assert(view.tilemap:getLayer(name))
        local visible = layer:getVisible()
        if previousLayerNames[index] ~= name or layerRefs[name] ~= layer and publishedLayerRefs[name] ~= layer
            or previousLayerVisibility[name] ~= visible then
            fullMap = true
        end
        currentLayerRefs[name] = layer
        currentLayerVisibility[name] = visible
    end
    layerRefs = currentLayerRefs
    previousLayerVisibility = currentLayerVisibility
    previousLayerNames = names
    previousWidth, previousHeight = view.width, view.height
    local changes = nil
    if not fullMap and pendingCellCount > 0 then
        changes = {}
        for _, name in ipairs(names) do
            for _, change in pairs(pendingCells[name] or {}) do
                changes[#changes + 1] = change
            end
        end
    end
    publishedLayerRefs = {}
    pendingCells = {}
    pendingCellCount = 0
    needsFullMap = false
    return fullMap, changes
end

return Tiles
