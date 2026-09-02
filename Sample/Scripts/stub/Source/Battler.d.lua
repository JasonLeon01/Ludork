---@meta Source.Battler

---@class Source.Battler.Battler
---@field attributes                      GlobalCore.AttributeSet
---@field private _abilitySystemComponent GlobalCore.AbilitySystemComponent
local Battler = {}

---@param attributes GlobalCore.AttributeSet
function Battler:init(attributes) end

---@return GlobalCore.AbilitySystemComponent
function Battler:getAbilitySystemComponent() end

---@param scene          Source.Scenes.SceneMap.SceneMap
---@param targetPosition sf.Vector2f
---@return number
function Battler:playAttackAnimationAt(scene, targetPosition) end

return Battler
