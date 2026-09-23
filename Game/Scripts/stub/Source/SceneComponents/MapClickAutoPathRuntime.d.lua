---@meta Source.SceneComponents.MapClickAutoPathRuntime

local MapClickAutoPathRuntime = {}

---@param gameMap            GameMap
---@param dangerState        Source.SceneComponents.MovementDangerState
---@param route              sf.Vector2i[]
---@param goal               sf.Vector2i | nil
---@param ignoredGoalEnemies Source.MapActors.Enemy[] | nil
---@param player             Source.MapActors.Player.Player
---@return boolean
function MapClickAutoPathRuntime.IsRouteInvalidatedByDanger(
    gameMap, dangerState, route, goal, ignoredGoalEnemies, player
) end

---@param route       sf.Vector2i[]
---@param destination sf.Vector2i
---@return sf.Vector2i[]
function MapClickAutoPathRuntime.GetTeleportPathPositions(route, destination) end

---@param route       sf.Vector2i[]
---@param destination sf.Vector2i
---@return integer
function MapClickAutoPathRuntime.GetInstantWalkCount(route, destination) end

---@param player    Source.MapActors.Player.Player
---@param walkCount integer
function MapClickAutoPathRuntime.TriggerInstantWalkStates(player, walkCount) end

---@param actor          Source.MapActors.Player.Player
---@param fromPos        sf.Vector2i
---@param goal           sf.Vector2i
---@param rotateWhenSame boolean
function MapClickAutoPathRuntime.SetActorDirection(actor, fromPos, goal, rotateWhenSame) end

return MapClickAutoPathRuntime
