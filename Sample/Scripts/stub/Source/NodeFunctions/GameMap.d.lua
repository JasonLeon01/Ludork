---@meta Source.NodeFunctions.GameMap

--- @brief Find an actor by tag on the current map.
---
--- - @param tag The tag to search for.
--- - @return The matching actor, or nil if not found.
---@param tag string
---@return Engine.Actor | nil
function GameMap.GetActorByTag(tag) end

--- @brief Get all actors on the current map.
---
--- - @return A flat list of all actors across all layers.
---@return Engine.Actor[]
function GameMap.GetAllActors() end

return GameMap
