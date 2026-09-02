local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local AbilitySystemComponent = GlobalCore.AbilitySystemComponent
local AttributeSet = GlobalCore.AttributeSet

local Battler = {}

local function onAttributeWrite(oldValue, newValue, abilitySystem, name)
    if abilitySystem ~= nil then
        abilitySystem:onAttributeWrite(name, oldValue, newValue)
    end
end

function Battler:init(attributes)
    assert(Class.isInstance(attributes, AttributeSet), "Battler requires a generated AttributeSet")
    self.attributes = attributes
    self._abilitySystemComponent = AbilitySystemComponent.new(self, attributes)
    self._attributeMonitorParams = {}
    for _, name in ipairs(attributes:getAttributeNames()) do
        local schema = attributes:getAttributeSchema(name)
        if schema.type == "int" or schema.type == "float" then
            local params = setmetatable({ self._abilitySystemComponent, name }, { __mode = "v" })
            self._attributeMonitorParams[#self._attributeMonitorParams + 1] = params
            Class.monitor(attributes, name, onAttributeWrite, params, true)
        end
    end
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
