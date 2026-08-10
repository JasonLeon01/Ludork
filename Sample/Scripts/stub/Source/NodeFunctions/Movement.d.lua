---@meta Source.NodeFunctions.Movement

---@class Source.NodeFunctions.Movement.Condition
---@field _actor          Engine.Actor | nil
---@field _startedEmitted boolean
---@field _finished       boolean
---@operator call: integer[]
local MovementCondition = {}

---@param actor Engine.Actor | nil
function MovementCondition:init(actor) end

---@return integer[]
function MovementCondition:poll() end

---@return boolean
function MovementCondition:isFinished() end

--- @brief Enable or disable movement for an actor identified by tag.
---
--- - @param tag The tag of the actor to modify.
--- - @param enabled Whether to enable movement.
---@param tag     string
---@param enabled boolean
function Movement.SetMoveEnabledByTag(tag, enabled) end

--- @brief Set an actor route and wait until movement finishes.
---
--- - @param actor The actor to move.
--- - @param route List of `sf.Vector2i` grid offsets, or `nil` to clear the route.
--- - @return A condition callable that emits Started immediately and Finished after movement ends.
---@param actor Engine.Actor
---@param route sf.Vector2i[]
---@return Source.NodeFunctions.Movement.Condition
function Movement.SetMoveRoute(actor, route) end

--- @brief Pathfind an actor to a destination and wait until movement finishes.
---
--- - @param actor The actor to move.
--- - @param destination Target map position as an `sf.Vector2i`.
--- - @return A condition callable that emits Started immediately and Finished after movement ends.
---@param actor       Engine.Actor
---@param destination sf.Vector2i
---@return Source.NodeFunctions.Movement.Condition
function Movement.SetAutoPathToDestination(actor, destination) end

--- @brief Pathfind an actor identified by tag to a destination and wait until movement finishes.
---
--- - @param tag The tag of the actor to move.
--- - @param destination Target map position as an `sf.Vector2i`.
--- - @return A condition callable that emits Started immediately and Finished after movement ends.
---@param tag         string
---@param destination sf.Vector2i
---@return Source.NodeFunctions.Movement.Condition
function Movement.SetAutoPathToDestinationByTag(tag, destination) end

return Movement
