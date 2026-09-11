---@meta Source.Battler

---@class Source.Battler.Battler
---@field attributes                      GlobalCore.AttributeSet
---@field private _abilitySystemComponent GlobalCore.AbilitySystemComponent
local Battler = {}

---@param attributes GlobalCore.AttributeSet
function Battler:init(attributes) end

---@return GlobalCore.AbilitySystemComponent
function Battler:getAbilitySystemComponent() end

---@param scene          GlobalCore.SceneBase
---@param targetPosition sf.Vector2f
---@return GlobalCore.Animation | nil
function Battler:playAttackAnimationAt(scene, targetPosition) end

return Battler
