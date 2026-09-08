local Engine = require("Engine")

local RenderSupport = {}

function RenderSupport.GetLightingCellRect(visible, limit, worldSize, activeLights)
    local minimumX = visible.x
    local minimumY = visible.y
    local maximumX = visible.x + visible.width
    local maximumY = visible.y + visible.height
    for _, entry in ipairs(activeLights) do
        local light = entry.light
        minimumX = math.min(minimumX, math.floor((light.position.x - light.radius) / Engine.GetCellSize()) - 1)
        minimumY = math.min(minimumY, math.floor((light.position.y - light.radius) / Engine.GetCellSize()) - 1)
        maximumX = math.max(maximumX, math.ceil((light.position.x + light.radius) / Engine.GetCellSize()) + 1)
        maximumY = math.max(maximumY, math.ceil((light.position.y + light.radius) / Engine.GetCellSize()) + 1)
    end
    local left = math.max(0, limit.x, minimumX)
    local top = math.max(0, limit.y, minimumY)
    local right = math.min(worldSize.x, limit.x + limit.width, maximumX)
    local bottom = math.min(worldSize.y, limit.y + limit.height, maximumY)
    if right <= left or bottom <= top then
        left = math.trunc(math.clamp(visible.x, 0, worldSize.x - 1))
        top = math.trunc(math.clamp(visible.y, 0, worldSize.y - 1))
        right = math.trunc(math.clamp(visible.x + visible.width, left + 1, worldSize.x))
        bottom = math.trunc(math.clamp(visible.y + visible.height, top + 1, worldSize.y))
    end
    ---@cast left integer
    ---@cast top integer
    ---@cast right integer
    ---@cast bottom integer
    return { x = left, y = top, width = right - left, height = bottom - top }
end

function RenderSupport.CreateTileMaskConfig(target, viewPosition, viewSize, viewRotation, region)
    local targetSize = target:getSize()
    local targetSizeFloat = sf.Vector2f.new(targetSize.x, targetSize.y)
    local regionSize = sf.Vector2f.new(region.width, region.height)
    local regionPosition = sf.Vector2f.new(region.x * Engine.GetCellSize(), region.y * Engine.GetCellSize())
    ---@cast targetSizeFloat sf.Vector2f
    ---@cast regionSize sf.Vector2f
    ---@cast regionPosition sf.Vector2f
    return {
        targetSize = targetSizeFloat,
        viewSize = viewSize,
        viewPosition = viewPosition,
        viewRotation = viewRotation,
        regionSize = regionSize,
        regionPosition = regionPosition
    }
end

function RenderSupport.TileMaskCacheKey(region, layerName)
    return region.path .. "\0" .. layerName
end

return RenderSupport
