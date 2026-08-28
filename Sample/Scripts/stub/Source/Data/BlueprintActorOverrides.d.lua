---@meta Source.Data.BlueprintActorOverrides

local BlueprintActorOverrides = {}

---@param actor Engine.Actor
function BlueprintActorOverrides.ApplyGeneration(actor) end

---@param actor   Engine.Actor
---@param changes table<string, Source.Data.ClassVarValue>
function BlueprintActorOverrides.ApplyChanges(actor, changes) end

return BlueprintActorOverrides
