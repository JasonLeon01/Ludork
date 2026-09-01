---@meta Global.Gameplay.AbilitySystemComponent

---@class Global.Gameplay.AttributeChange
---@field source  "Base" | "Effect" | "Constraint"
---@field force   boolean
---@field oldBase number | Class.MissingValue | nil
---@field newBase number | nil

---@class Global.Gameplay.AbilitySystemComponent
---@field new fun(owner: any, attributeSet: Global.Gameplay.AttributeSet): Global.Gameplay.AbilitySystemComponent
local AbilitySystemComponent = {}

---@param owner        any
---@param attributeSet Global.Gameplay.AttributeSet
function AbilitySystemComponent:init(owner, attributeSet) end

---@return any
function AbilitySystemComponent:getOwner() end

---@return Global.Gameplay.AttributeSet
function AbilitySystemComponent:getAttributeSet() end

---@param name string
---@return number
function AbilitySystemComponent:getNumericAttribute(name) end

---@param name string
---@return number
function AbilitySystemComponent:getNumericAttributeBase(name) end

---@param name  string
---@param value number
function AbilitySystemComponent:setNumericAttributeBase(name, value) end

---@param values table<string, number>
function AbilitySystemComponent:setNumericAttributeBases(values) end

---@return table<string, number>
function AbilitySystemComponent:getNumericAttributeBases() end

---@param name     string
---@param callback fun(oldValue: number, newValue: number, change: Global.Gameplay.AttributeChange, ...)
---@param params?  any[]
function AbilitySystemComponent:addAttributeChangeListener(name, callback, params) end

---@param name     string
---@param callback fun(value: number, abilitySystem: Global.Gameplay.AbilitySystemComponent, resolvedValues: table<string, number>): number | nil
function AbilitySystemComponent:setNumericAttributeConstraint(name, callback) end

---@param ability    Global.Gameplay.GameplayAbility
---@param sourceKey? any
---@return Global.Gameplay.GameplayAbilitySpec
function AbilitySystemComponent:giveAbility(ability, sourceKey) end

---@param sourceKey any
function AbilitySystemComponent:removeAbilitiesBySource(sourceKey) end

---@param abilityID  string
---@param eventData? Global.Gameplay.GameplayEventData
---@return Global.Gameplay.GameplayAbilityResult
function AbilitySystemComponent:tryActivateAbility(abilityID, eventData) end

---@param eventData Global.Gameplay.GameplayEventData
---@return Global.Gameplay.GameplayAbilityResult[]
function AbilitySystemComponent:handleGameplayEvent(eventData) end

---@param spec Global.Gameplay.GameplayEffectSpec
---@return integer | nil
function AbilitySystemComponent:applyGameplayEffectSpec(spec) end

---@param spec Global.Gameplay.GameplayEffectSpec
---@return boolean
function AbilitySystemComponent:validateGameplayEffectSpec(spec) end

---@param handle  integer
---@param stacks? integer
---@return boolean
function AbilitySystemComponent:removeActiveGameplayEffect(handle, stacks) end

---@param effectID string
---@return integer
function AbilitySystemComponent:getActiveEffectStacks(effectID) end

---@return Global.Gameplay.ActiveGameplayEffect[]
function AbilitySystemComponent:getActiveGameplayEffects() end

---@param tag string
---@return boolean
function AbilitySystemComponent:hasMatchingGameplayTag(tag) end

---@return integer
function AbilitySystemComponent:getRevision() end

return AbilitySystemComponent
