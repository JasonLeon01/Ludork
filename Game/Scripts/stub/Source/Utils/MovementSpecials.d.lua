---@meta Source.Utils.MovementSpecials

---@class Source.Utils.MovementSpecials.DangerSource
---@field enemy   Source.MapActors.Enemy
---@field special string
---@field damage  integer

---@class Source.Utils.MovementSpecials.PreviewContext
---@field player        Source.MapActors.Player.Player
---@field damageByEnemy table<Source.MapActors.Enemy, integer>
---@field radiusByEnemy table<Source.MapActors.Enemy, integer>

local MovementSpecials = {}

---@param gameMap GameMap
---@return Source.MapActors.Enemy[]
function MovementSpecials.CollectEnemies(gameMap) end

---@brief Capture movement damage and query bounds for an unchanged player/enemy snapshot.
--- Pass movement-hazard enemies, as returned by CollectEnemies.
--- Every ability subscribed to Event.Movement.QueryHazard must also answer
--- Event.Movement.QueryHazardBounds with a successful MovementHazardBounds result
--- containing a non-negative integer radius. Missing or invalid bounds raise.
--- The largest radius covers every contributing cell in an inclusive square
--- around that enemy's map anchor; per-cell Preview still resolves exact damage.
--- Recreate the context when player/enemy state or enemy membership changes.
---@param enemies Source.MapActors.Enemy[]
---@param player  Source.MapActors.Player.Player
---@return Source.Utils.MovementSpecials.PreviewContext
function MovementSpecials.CreatePreviewContext(enemies, player) end

---@param enemies         Source.MapActors.Enemy[]
---@param player          Source.MapActors.Player.Player
---@param playerPosition  sf.Vector2i
---@param ignoredEnemies? Source.MapActors.Enemy[]
---@param previewContext? Source.Utils.MovementSpecials.PreviewContext
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Preview(enemies, player, playerPosition, ignoredEnemies, previewContext) end

---@param player        Source.MapActors.Player.Player
---@param pathPositions sf.Vector2i[]
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.Commit(player, pathPositions) end

---@param player         Source.MapActors.Player.Player
---@param pathPositions? sf.Vector2i[]
---@return GlobalCore.GameplayAbilityResult
function MovementSpecials.NotifyPlayerMovementFinished(player, pathPositions) end

return MovementSpecials
