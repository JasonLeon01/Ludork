---@meta Internal.LiveDebug.Snapshot

local Snapshot = {}

--- Resolve a process-local runtime ID only while the Actor is alive and belongs to the current debug map.
---@param id   string
---@param view Internal.LiveDebug.View
---@return Engine.Actor | nil
function Snapshot.FindActor(id, view) end

--- Collect visible and hidden live Actors, including the player and dynamic descendants.
---@param view Internal.LiveDebug.View
---@return table<string, Internal.LiveDebug.Actor[]>
function Snapshot.Actors(view) end

--- Serialize the current runtime tile layers using the editor map schema.
---@param view   Internal.LiveDebug.View
---@param actors table<string, Internal.LiveDebug.Actor[]>
---@return Internal.LiveDebug.Map
function Snapshot.Map(view, actors) end

return Snapshot
