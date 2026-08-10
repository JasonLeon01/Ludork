---@meta Source.SceneComponents.MapClickAutoPath
---@class MapClickAutoPath.Plan
---@field routeSteps   sf.Vector2i[]
---@field route        sf.Vector2i[]
---@field goalPassable boolean

---@class MapClickAutoPath.Path
---@field routeSteps sf.Vector2i[]
---@field route sf.Vector2i[]

---@class Source.SceneComponents.MapClickAutoPath: ComponentBase
---@field _parent GameMap
---@field _routeState PathRouteState
---@field _autoPathing boolean
---@field _pendingGoals sf.Vector2i[]
local MapClickAutoPath = {}

---@param gameMap GameMap
---@param routeState PathRouteState
---@return Source.SceneComponents.MapClickAutoPath
function MapClickAutoPath.new(...) end

--- @brief Component that pathfinds and moves an actor to the clicked tile.
---
--- This component listens for mouse clicks, finds a path to the
--- clicked tile, and moves the player actor along that path.

--- @brief Initialize the MapClickAutoPath component.
--- - gameMap: The game map this component operates on.
--- - routeState: The path route state to use for path preview.
---@param gameMap    GameMap
---@param routeState PathRouteState
function MapClickAutoPath:init(gameMap, routeState) end

--- @brief Handle mouse click input to set pathfinding goals.
---
--- This method checks for left mouse button clicks and adds
--- the clicked map position to the pending goals queue.
---@param deltaTime number
function MapClickAutoPath:onLateTick(deltaTime) end

--- @brief Update pathfinding and movement logic.
---
--- This method processes pending goals, builds pathfinding plans,
--- and moves the player actor along the calculated paths.
---@param deltaTime number
function MapClickAutoPath:onTick(deltaTime) end

return MapClickAutoPath
