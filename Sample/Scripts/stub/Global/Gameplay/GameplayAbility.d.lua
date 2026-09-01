---@meta Global.Gameplay.GameplayAbility

---@class Global.Gameplay.GameplayAbility
---@field id           string
---@field priority     integer
---@field abilityTags  string[]
---@field requiredTags string[]
---@field blockedTags  string[]
---@field triggerTags  string[]
---@field new          fun(values?: table<string, any>): Global.Gameplay.GameplayAbility
local GameplayAbility = {}

---@param values? table<string, any>
function GameplayAbility:init(values) end

---@param abilitySystem Global.Gameplay.AbilitySystemComponent
---@param eventData     Global.Gameplay.GameplayEventData
---@return Global.Gameplay.GameplayAbilityResult
function GameplayAbility:canActivate(abilitySystem, eventData) end

---@param abilitySystem Global.Gameplay.AbilitySystemComponent
---@param eventData     Global.Gameplay.GameplayEventData
---@return Global.Gameplay.GameplayAbilityResult
function GameplayAbility:calculate(abilitySystem, eventData) end

---@param abilitySystem Global.Gameplay.AbilitySystemComponent
---@param eventData     Global.Gameplay.GameplayEventData
---@return Global.Gameplay.GameplayAbilityResult
function GameplayAbility:activate(abilitySystem, eventData) end

return GameplayAbility
