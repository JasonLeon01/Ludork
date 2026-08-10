---@meta Source.MovementSpecials

---@class Source.MovementSpecials.FinishedPayload
---@field player        Source.Player.Player
---@field pathPositions sf.Vector2i[] | nil

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
