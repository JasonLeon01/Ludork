---@meta Source.Data.BlueprintActorOverrides

local BlueprintActorOverrides = {}

---@param actor Engine.Actor
function BlueprintActorOverrides.ApplyGeneration(actor) end

---@param actor   Engine.Actor
---@param changes table<string, Source.Data.ClassVarValue>
function BlueprintActorOverrides.ApplyChanges(actor, changes) end

---@param actorType  Class.ClassType<any>
---@param key        string
---@param value      Source.Data.ClassVarValue
---@param descriptor any
---@return any
function BlueprintActorOverrides.ResolveValue(actorType, key, value, descriptor) end

---@param descriptor any
---@return boolean
function BlueprintActorOverrides.IsBlueprintOnly(descriptor) end

return BlueprintActorOverrides
