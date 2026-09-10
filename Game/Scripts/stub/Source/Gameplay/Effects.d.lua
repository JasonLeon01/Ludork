---@meta Source.Gameplay.Effects

local Effects = {}

---@param effectID   string
---@param attribute  string
---@param operation  "Add" | "Multiply" | "Override"
---@param magnitude  number
---@param eventData? GlobalCore.GameplayEventData
---@return GlobalCore.GameplayEffectSpec
function Effects.CreateInstantModifierSpec(effectID, attribute, operation, magnitude, eventData) end

---@param target     Source.Battler.Battler
---@param effectID   string
---@param attribute  string
---@param operation  'Add' | 'Multiply' | 'Override'
---@param magnitude  number
---@param eventData? GlobalCore.GameplayEventData
---@return integer | nil
function Effects.ApplyInstantModifier(target, effectID, attribute, operation, magnitude, eventData) end

---@param equipID  string
---@param slot     string
---@param attrPlus table<string, integer>
---@return GlobalCore.GameplayEffect
function Effects.CreateEquipmentEffect(equipID, slot, attrPlus) end

---@param stateID string
---@return GlobalCore.GameplayEffect
function Effects.CreateStateEffect(stateID) end

---@param target     Source.Battler.Battler
---@param stateID    string
---@param stacks?    integer
---@param eventData? GlobalCore.GameplayEventData
---@return integer
function Effects.ApplyState(target, stateID, stacks, eventData) end

---@param stateID    string
---@param stacks?    integer
---@param eventData? GlobalCore.GameplayEventData
---@return GlobalCore.GameplayEffectSpec
function Effects.CreateStateSpec(stateID, stacks, eventData) end

---@param target  Source.Battler.Battler
---@param stateID string
---@return boolean
function Effects.RemoveState(target, stateID) end

---@param target  Source.Battler.Battler
---@param stateID string
---@param stacks? integer
---@return boolean
function Effects.ReduceState(target, stateID, stacks) end

---@param target Source.Battler.Battler
function Effects.ClearStates(target) end

---@param target Source.Battler.Battler
---@return table<string, integer>
function Effects.GetStateStacks(target) end

---@param target Source.Battler.Battler
---@return string[]
function Effects.GetStateIDs(target) end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param effectID      string
---@return integer | nil
function Effects.FindActiveEffectHandle(abilitySystem, effectID) end

return Effects
