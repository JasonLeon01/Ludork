---@meta Global.Gameplay.GameplayEffectSpec

---@class Global.Gameplay.GameplayEffectSpec
---@field effect    Global.Gameplay.GameplayEffect
---@field eventData Global.Gameplay.GameplayEventData | nil
---@field stacks    integer
---@field sourceKey any
---@field new       fun(effect: Global.Gameplay.GameplayEffect, eventData?: Global.Gameplay.GameplayEventData, stacks?: integer, sourceKey?: any): Global.Gameplay.GameplayEffectSpec
local GameplayEffectSpec = {}

---@param effect     Global.Gameplay.GameplayEffect
---@param eventData? Global.Gameplay.GameplayEventData
---@param stacks?    integer
---@param sourceKey? any
function GameplayEffectSpec:init(effect, eventData, stacks, sourceKey) end

return GameplayEffectSpec
