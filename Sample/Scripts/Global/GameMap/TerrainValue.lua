local TerrainValue = {}

---@param tileID Global.GameMap.TerrainTileID
---@return Global.GameMap.TerrainTileID
function TerrainValue.Normalise(tileID)
    if tileID == nil then
        return nil
    elseif type(tileID) == "string" then
        return bool(tileID) and tileID or nil
    end
    assert(
        type(tileID) == "number" and math.type(tileID) == "integer",
        "terrain tile ID must be an integer, string, or nil"
    )
    return tileID
end

return TerrainValue
