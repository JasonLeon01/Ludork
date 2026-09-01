---@meta Global.Gameplay.ActiveGameplayEffect

---@class Global.Gameplay.ActiveGameplayEffect
---@field handle              integer
---@field spec                Global.Gameplay.GameplayEffectSpec
---@field stacks              integer
---@field applicationOrder    integer
---@field grantedAbilitySpecs Global.Gameplay.GameplayAbilitySpec[]
---@field new                 fun(handle: integer, spec: Global.Gameplay.GameplayEffectSpec, applicationOrder: integer): Global.Gameplay.ActiveGameplayEffect
local ActiveGameplayEffect = {}

---@param handle           integer
---@param spec             Global.Gameplay.GameplayEffectSpec
---@param applicationOrder integer
function ActiveGameplayEffect:init(handle, spec, applicationOrder) end

return ActiveGameplayEffect
