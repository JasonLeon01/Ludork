---@meta Source.NodeFunctions.Gameplay

local Gameplay = {}

---@return GlobalCore.GameplayEventData
function Gameplay.GetContext() end

---@return any
function Gameplay.GetSource() end

---@return any
function Gameplay.GetTarget() end

---@return string
function Gameplay.GetEventTag() end

---@return table<string, any>
function Gameplay.GetPayload() end

---@param target Source.Battler.Battler
---@param tag    string
---@return boolean
function Gameplay.HasTag(target, tag) end

---@param target    Source.Battler.Battler
---@param attribute string
---@return number
function Gameplay.GetNumericAttribute(target, attribute) end

---@param target    Source.Battler.Battler
---@param attribute string
---@param value     number
function Gameplay.SetNumericAttributeBase(target, attribute, value) end

---@param target    Source.Battler.Battler
---@param attribute string
---@param magnitude number
function Gameplay.ApplyAttributeDelta(target, attribute, magnitude) end

---@param target  Source.Battler.Battler
---@param stateID string
---@param stacks? integer
---@return integer
function Gameplay.ApplyState(target, stateID, stacks) end

---@param target  Source.Battler.Battler
---@param stateID string
---@return boolean
function Gameplay.RemoveState(target, stateID) end

---@param target  Source.Battler.Battler
---@param stateID string
---@param stacks? integer
---@return boolean
function Gameplay.ReduceState(target, stateID, stacks) end

---@param stateID string
---@return boolean
function Gameplay.RemovePlayerState(stateID) end

---@param stateID string
---@param stacks? integer
---@return boolean
function Gameplay.ReducePlayerState(stateID, stacks) end

---@param target   Source.Battler.Battler
---@param eventTag string
---@param payload? table<string, any>
---@return GlobalCore.GameplayAbilityResult[]
function Gameplay.SendEvent(target, eventTag, payload) end

---@param target     Source.Battler.Battler
---@param effect     GlobalCore.GameplayEffect
---@param stacks?    integer
---@param sourceKey? any
---@return integer | nil
function Gameplay.ApplyEffect(target, effect, stacks, sourceKey) end

return Gameplay
