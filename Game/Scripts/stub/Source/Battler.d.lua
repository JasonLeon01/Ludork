---@meta Source.Battler

---@class Source.Battler.Battler
---@field attributes                      GlobalCore.AttributeSet
---@field private _abilitySystemComponent GlobalCore.AbilitySystemComponent
local Battler = {}

---@param attributes GlobalCore.AttributeSet
function Battler:init(attributes) end

---@return GlobalCore.AbilitySystemComponent
function Battler:getAbilitySystemComponent() end

--- Returns the numeric Current value, or Base when base is true.
---@param name  string
---@param base? boolean
---@return integer | number
function Battler:getAttr(name, base) end

--- Sets numeric Base and recalculates Current through the Ability System.
---@param name  string
---@param value integer | number
function Battler:setAttr(name, value) end

--- Adds delta to numeric Base without baking active modifiers into it.
---@param name  string
---@param delta integer | number
function Battler:addAttr(name, delta) end

---@param scene          GlobalCore.SceneBase
---@param targetPosition sf.Vector2f
---@return GlobalCore.Animation | nil
function Battler:playAttackAnimationAt(scene, targetPosition) end

return Battler
