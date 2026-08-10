local Engine = require("Engine")
local ComponentBase = require("Global.Components.ComponentBase")
local Pool = require("Global.Pool")

local PathPreviewComponent = {}

function PathPreviewComponent:init(gameMap, routeState)
    super(PathPreviewComponent, self).init(gameMap)
    self._routeState = routeState
    self._fillColour = sf.Color.new(80, 180, 255, 110)
    self._outlineColour = sf.Color.new(120, 220, 255, 180)
    self._padding = math.max(1.0, Engine.CellSize * 0.12)
    local size = math.max(1.0, Engine.CellSize - self._padding * 2.0)
    self._rectangleSize = sf.Vector2f.new(size, size)
end

function PathPreviewComponent:onRender(camera)
    local route = self._routeState:getRoute()
    if not bool(route) then
        return
    end
    local cellSize = Engine.CellSize
    local firstCell = route[1]
    ---@cast firstCell - nil
    local position = Pool.Get("sf.Vector2f", sf.Vector2f, {
        x = firstCell.x * cellSize + self._padding,
        y = firstCell.y * cellSize + self._padding
    })
    for index, cell in ipairs(route) do
        if index > 1 then
            position.x = cell.x * cellSize + self._padding
            position.y = cell.y * cellSize + self._padding
        end
        local rectangle = sf.RectangleShape.new(self._rectangleSize)
        rectangle:setPosition(position)
        rectangle:setFillColor(self._fillColour)
        rectangle:setOutlineColor(self._outlineColour)
        rectangle:setOutlineThickness(1.0)
        camera:render(rectangle)
    end
    Pool.Put("sf.Vector2f", position)
end

return class(PathPreviewComponent, ComponentBase)
