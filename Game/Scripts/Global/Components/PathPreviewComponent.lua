local Engine = require("Engine")
local ComponentBase = require("Global.Components.ComponentBase")
local Pool = require("Global.Pool")

---@class (partial) Global.Components.PathPreviewComponent
local PathPreviewComponent = {
    _DEFAULT_FILL_COLOUR = sf.Color.new(80, 180, 255, 110),
    _DEFAULT_OUTLINE_COLOUR = sf.Color.new(120, 220, 255, 180),
    _CELL_PADDING_RATIO = 0.12,
    _MINIMUM_PADDING = 1.0,
    _OUTLINE_THICKNESS = 1.0
}

function PathPreviewComponent:init(gameMap, routeState)
    super(PathPreviewComponent, self).init(gameMap)
    ---@type PathRouteState
    self._routeState = routeState
    self._fillColour = PathPreviewComponent._DEFAULT_FILL_COLOUR:copy()
    self._outlineColour = PathPreviewComponent._DEFAULT_OUTLINE_COLOUR:copy()
    self._padding = math.max(
        PathPreviewComponent._MINIMUM_PADDING, Engine.GetCellSize() * PathPreviewComponent._CELL_PADDING_RATIO
    )
    local size = math.max(1.0, Engine.GetCellSize() - self._padding * 2.0)
    self._rectangleSize = sf.Vector2f.new(size, size)
    self._rectangle = sf.RectangleShape.new(self._rectangleSize)
    self._rectangle:setFillColor(self._fillColour)
    self._rectangle:setOutlineColor(self._outlineColour)
    self._rectangle:setOutlineThickness(PathPreviewComponent._OUTLINE_THICKNESS)
    self._cachedRouteRevision = -1
    ---@type sf.Vector2i[]
    self._cachedRoute = {}
end

function PathPreviewComponent:onRender(camera)
    local routeRevision = self._routeState:getRevision()
    if routeRevision ~= self._cachedRouteRevision then
        self._cachedRoute = self._routeState:getRoute()
        self._cachedRouteRevision = routeRevision
    end
    if not bool(self._cachedRoute) then
        return
    end
    local cellSize = Engine.GetCellSize()
    ---@diagnostic disable: need-check-nil
    local position = Pool.Get("sf.Vector2f", sf.Vector2f, {
        x = self._cachedRoute[1].x * cellSize + self._padding,
        y = self._cachedRoute[1].y * cellSize + self._padding
    })
    ---@diagnostic enable: need-check-nil
    for index, cell in ipairs(self._cachedRoute) do
        if index > 1 then
            position.x = cell.x * cellSize + self._padding
            position.y = cell.y * cellSize + self._padding
        end
        self._rectangle:setPosition(position)
        camera:render(self._rectangle)
    end
    Pool.Put("sf.Vector2f", position)
end

return class(PathPreviewComponent, ComponentBase)
