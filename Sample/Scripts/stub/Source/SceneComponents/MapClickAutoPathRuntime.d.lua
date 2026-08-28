---@meta Source.SceneComponents.MapClickAutoPathRuntime

local MapClickAutoPathRuntime = {}

---@param gameMap GameMap
---@param goal    sf.Vector2i
---@return boolean
function MapClickAutoPathRuntime.HasTeleporterAt(gameMap, goal) end

---@param autoPath Source.SceneComponents.MapClickAutoPath
---@param player   Source.Player.Player
---@return boolean
function MapClickAutoPathRuntime.IsRouteInvalidatedByDanger(autoPath, player) end

---@param route       sf.Vector2i[]
---@param destination sf.Vector2i
---@return sf.Vector2i[]
function MapClickAutoPathRuntime.GetTeleportPathPositions(route, destination) end

---@param route       sf.Vector2i[]
---@param destination sf.Vector2i
---@return integer
function MapClickAutoPathRuntime.GetInstantWalkCount(route, destination) end

---@param player    Source.Player.Player
---@param walkCount integer
function MapClickAutoPathRuntime.TriggerInstantWalkStates(player, walkCount) end

---@param actor          Source.Player.Player
---@param fromPos        sf.Vector2i
---@param goal           sf.Vector2i
---@param rotateWhenSame boolean
function MapClickAutoPathRuntime.SetActorDirection(actor, fromPos, goal, rotateWhenSame) end

return MapClickAutoPathRuntime
