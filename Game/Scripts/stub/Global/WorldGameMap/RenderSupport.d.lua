---@meta Global.WorldGameMap.RenderSupport

local RenderSupport = {}

---@param visible      Global.WorldGeometry.CellRect
---@param limit        Global.WorldGeometry.CellRect
---@param worldSize    sf.Vector2u
---@param activeLights Global.GameMap.ActiveLight[]
---@return Global.WorldGeometry.CellRect
function RenderSupport.GetLightingCellRect(visible, limit, worldSize, activeLights) end

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
