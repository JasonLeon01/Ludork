---@meta Global.Data.BlueprintActorOverrides

local BlueprintActorOverrides = {}

---@param actor Engine.Actor
function BlueprintActorOverrides.ApplyGeneration(actor) end

---@param actor          Engine.Actor
---@param changes        table<string, Global.Data.ClassVarValue>
---@param configResolver fun(configName: string, settingName: string): string
function BlueprintActorOverrides.ApplyChanges(actor, changes, configResolver) end

---@param actorType      Class.ClassType<any>
---@param key            string
---@param value          Global.Data.ClassVarValue
---@param descriptor     any
---@param configResolver fun(configName: string, settingName: string): string
---@return any
function BlueprintActorOverrides.ResolveValue(actorType, key, value, descriptor, configResolver) end

---@param descriptor any
---@return boolean
function BlueprintActorOverrides.IsBlueprintOnly(descriptor) end

return BlueprintActorOverrides
