---@meta Source.Battler

---@class Source.Battler.Battler
---@field attributes                      Global.Gameplay.AttributeSet
---@field private _abilitySystemComponent Global.Gameplay.AbilitySystemComponent
local Battler = {}

---@param attributes Global.Gameplay.AttributeSet
function Battler:init(attributes) end

---@return Global.Gameplay.AbilitySystemComponent
function Battler:getAbilitySystemComponent() end

---@param scene          Source.Scenes.SceneMap.SceneMap
---@param targetPosition sf.Vector2f
---@return number
function Battler:playAttackAnimationAt(scene, targetPosition) end

return Battler
