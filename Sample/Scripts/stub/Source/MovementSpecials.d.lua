---@meta Source.MovementSpecials

---@class Source.MovementSpecials.FinishedPayload
---@field player        Source.Player.Player
---@field pathPositions sf.Vector2i[] | nil

---@class Source.MovementSpecials.DangerSource
---@field enemy   Source.Enemy
---@field special string
---@field damage  integer

--- @brief Calculate movement-special damage at one map cell without running gameplay side effects.
---
--- Ignored enemies keep participating in Flank geometry, but their own damage
--- contribution is removed so the paired enemy remains a danger source.
---@param enemies        Source.Enemy[]
---@param player         Source.Player.Player
---@param playerPosition sf.Vector2i
---@param ignoredEnemies Source.Enemy[] | nil
---@return integer, Source.MovementSpecials.DangerSource[]
function MovementSpecials.CalculateDangerAtPosition(enemies, player, playerPosition, ignoredEnemies) end

--- @brief Register movement-special handlers on the shared EventBus.
function MovementSpecials.registerHandlers() end

--- @brief Notify listeners that the player has finished a movement sequence.
---
--- - @param player The player that just stopped moving.
--- - @param pathPositions Optional arrived cells to evaluate. When omitted, uses
---   cells recorded during walking, or the player's current cell as fallback.
---@param player        Source.Player.Player
---@param pathPositions sf.Vector2i[] | nil
function MovementSpecials.notifyPlayerMovementFinished(player, pathPositions) end

return MovementSpecials
