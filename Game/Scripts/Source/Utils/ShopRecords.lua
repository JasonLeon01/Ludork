local Engine = require("Engine")
local Data = require("Global.Data")
local MapBuilder = require("Source.SceneComponents.MapBuilder")
local MapConstants = require("Source.Configs.MapConstants")

local ShopRecords = {}

---@param data Global.GameMap.MapData
---@param tag  string
---@return Engine.Actor | nil
local function createMapActor(data, tag)
    for _, layerName in ipairs(data.layerOrder) do
        for _, record in ipairs(data.actors[layerName] or {}) do
            if record.tag == tag then
                local changes = data.BPClassVarChanged ~= nil and data.BPClassVarChanged[tag] or nil
                return Data.GenActorFromData(record, layerName, changes)
            end
        end
    end
    return nil
end

function ShopRecords.CreateActor(mapPath, tag)
    local builder = MapBuilder.new()
    local resolvedPath = builder:resolveMapPath(mapPath)
    if not Engine.dataExists(MapBuilder.GetMapDataPath(resolvedPath)) then
        return nil
    end
    local _, data = builder:loadMapData(resolvedPath)
    if data.type ~= MapConstants.WORLD_MAP_TYPE then
        ---@cast data Global.GameMap.MapData
        return createMapActor(data, tag)
    end
    ---@cast data Global.WorldGameMap.WorldMapData
    for _, region in ipairs(data.regions) do
        local _, child = builder:loadMapData(region.path, resolvedPath)
        ---@cast child Global.GameMap.MapData
        local actor = createMapActor(child, tag)
        if actor ~= nil then
            actor:setMapPosition(actor:getMapPosition() + sf.Vector2i.new(region.x, region.y))
            return actor
        end
    end
    return nil
end

return ShopRecords
