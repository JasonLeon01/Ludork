local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local AbilitySystemComponent = require("Global.Gameplay.AbilitySystemComponent")
local AttributeSet = require("Global.Gameplay.AttributeSet")

local Battler = {}

function Battler:init(attributes)
    assert(Class.isInstance(attributes, AttributeSet), "Battler requires a generated AttributeSet")
    self.attributes = attributes
    self._abilitySystemComponent = AbilitySystemComponent.new(self, attributes)
end

function Battler:getAbilitySystemComponent()
    return self._abilitySystemComponent
end

function Battler:playAttackAnimationAt(scene, targetPosition)
    if not bool(self.attributes.ANIMATION_KEY) then
        return 0.0
    end
    local animationData = Data.GetAnimation(self.attributes.ANIMATION_KEY)
    local Animation = GlobalCore.Animation
    local animation = Animation.new(animationData, true)
    local halfCell = Engine.CellSize * 0.5
    animation:setPosition(sf.Vector2f.new(targetPosition.x + halfCell, targetPosition.y + halfCell))
    scene:addAnim(animation)
    return animation:getVisualDuration()
end

return class(Battler)
