---@meta Source.MovementSpecials

---@class Source.MovementSpecials.DangerSource
---@field enemy   Source.Enemy
---@field special string
---@field damage  integer

local MovementSpecials = {}

---@param enemies         Source.Enemy[]
---@param player          Source.Player.Player
---@param playerPosition  sf.Vector2i
---@param ignoredEnemies? Source.Enemy[]
---@return Global.Gameplay.GameplayAbilityResult
function MovementSpecials.Preview(enemies, player, playerPosition, ignoredEnemies) end

---@param player        Source.Player.Player
---@param pathPositions sf.Vector2i[]
---@return Global.Gameplay.GameplayAbilityResult
function MovementSpecials.Commit(player, pathPositions) end

---@param player         Source.Player.Player
---@param pathPositions? sf.Vector2i[]
---@return Global.Gameplay.GameplayAbilityResult
function MovementSpecials.NotifyPlayerMovementFinished(player, pathPositions) end

return MovementSpecials
