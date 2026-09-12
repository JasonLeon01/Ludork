---@meta Source.LiveDebug.Snapshot

local Snapshot = {}

--- Resolve a process-local runtime ID only while the Actor is alive and belongs to the current debug map.
---@param id   string
---@param view Source.LiveDebug.View
---@return Engine.Actor | nil
function Snapshot.FindActor(id, view) end

--- Collect visible and hidden live Actors, including the player and dynamic descendants.
---@param view Source.LiveDebug.View
---@return table<string, Source.LiveDebug.Actor[]>
function Snapshot.Actors(view) end

--- Serialize the current runtime tile layers using the editor map schema.
---@param view   Source.LiveDebug.View
---@param actors table<string, Source.LiveDebug.Actor[]>
---@return Source.LiveDebug.Map
function Snapshot.Map(view, actors) end

return Snapshot
