---@meta Global.WorldGameMap.RegionOrdering

local RegionOrdering = {}

---@param world   Global.WorldGameMap.WorldGameMap
---@param regions Source.SceneComponents.WorldRegionData[]
function RegionOrdering.SortByDemand(world, regions) end

---@param regions Source.SceneComponents.WorldRegionData[]
function RegionOrdering.SortByLastUsed(regions) end

---@param regions Source.SceneComponents.WorldRegionData[]
function RegionOrdering.SortByIndex(regions) end

return RegionOrdering
