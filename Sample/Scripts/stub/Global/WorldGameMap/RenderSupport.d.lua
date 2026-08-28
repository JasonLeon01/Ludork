---@meta Global.WorldGameMap.RenderSupport

local RenderSupport = {}

---@param world        Global.WorldGameMap.WorldGameMap
---@param activeLights Global.GameMap.ActiveLight[]
---@return Global.WorldGeometry.CellRect
function RenderSupport.GetLightingCellRect(world, activeLights) end

---@param target       sf.RenderTexture
---@param viewPosition sf.Vector2f
---@param viewSize     sf.Vector2f
---@param viewRotation number
---@param region       Source.SceneComponents.WorldRegionData
---@return Global.GameMap.WorldTileMaskConfig
function RenderSupport.CreateTileMaskConfig(target, viewPosition, viewSize, viewRotation, region) end

---@param region    Source.SceneComponents.WorldRegionData
---@param layerName string
---@return string
function RenderSupport.TileMaskCacheKey(region, layerName) end

return RenderSupport
