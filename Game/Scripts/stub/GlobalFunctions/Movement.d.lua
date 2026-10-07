---@meta GlobalFunctions.Movement

local Movement = {}

---@brief Enable or disable movement for an actor identified by tag.
---
--- - @param tag The tag of the actor to modify.
--- - @param enabled Whether to enable movement.
---@param tag     string
---@param enabled boolean
function Movement.SetMoveEnabledByTag(tag, enabled) end

---@brief Set an actor route and wait until movement finishes.
---
--- - @param actor The actor to move.
--- - @param route List of `sf.Vector2i` grid offsets, or `nil` to clear the route.
--- - @return An operation that emits Started and completes with Finished after movement ends.
---@param actor Engine.Actor
---@param route sf.Vector2i[]
---@return Engine.AsyncOperation
function Movement.SetMoveRoute(actor, route) end

---@brief Pathfind an actor to a destination and wait until movement finishes.
---
--- - @param actor The actor to move.
--- - @param destination Target map position as an `sf.Vector2i`.
--- - @return An operation that emits Started and completes with Finished after movement ends.
---@param actor       Engine.Actor
---@param destination sf.Vector2i
---@return Engine.AsyncOperation
function Movement.SetAutoPathToDestination(actor, destination) end

---@brief Pathfind an actor identified by tag to a destination and wait until movement finishes.
---
--- - @param tag The tag of the actor to move.
--- - @param destination Target map position as an `sf.Vector2i`.
--- - @return An operation that emits Started and completes with Finished after movement ends.
---@param tag         string
---@param destination sf.Vector2i
---@return Engine.AsyncOperation
function Movement.SetAutoPathToDestinationByTag(tag, destination) end

return Movement
