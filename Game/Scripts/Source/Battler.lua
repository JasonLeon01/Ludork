local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local EventKeys = require("Source.Configs.EventKeys")

local AbilitySystemComponent = GlobalCore.AbilitySystemComponent
local AttributeSet = GlobalCore.AttributeSet

local Battler = {}

local function onAttributeWrite(oldValue, newValue, abilitySystem, name)
    if abilitySystem ~= nil then
        abilitySystem:onAttributeWrite(name, oldValue, newValue)
    end
end

local function onNumericAttributeChanged(oldValue, newValue, _change, battler, name)
    if battler == nil then
        return
    end
    if battler.getLoading ~= nil and battler:getLoading() then
        return
    end
    if oldValue == newValue then
        return
    end
    Engine.publish(EventKeys.AbilitySystemChanged, {
        owner = battler,
        kind = EventKeys.AbilitySystemChangeKind.Attribute,
        name = name
    })
end

function Battler:init(attributes)
    assert(Class.isInstance(attributes, AttributeSet), "Battler requires a generated AttributeSet")
    self.attributes = attributes
    self._abilitySystemComponent = AbilitySystemComponent.new(self, attributes)
    self._attributeMonitorParams = {}
    self._attributeChangeListenerParams = {}
    for _, name in ipairs(attributes:getAttributeNames()) do
        local schema = attributes:getAttributeSchema(name)
        if schema.type == "int" or schema.type == "float" then
            local params = setmetatable({ self._abilitySystemComponent, name }, { __mode = "v" })
            self._attributeMonitorParams[#self._attributeMonitorParams + 1] = params
            Class.monitor(attributes, name, onAttributeWrite, params, true)
            local listenerParams = setmetatable({ self, name }, { __mode = "v" })
            self._attributeChangeListenerParams[#self._attributeChangeListenerParams + 1] = listenerParams
            self._abilitySystemComponent:addAttributeChangeListener(name, onNumericAttributeChanged, listenerParams)
        end
    end
end

function Battler:getAbilitySystemComponent()
    return self._abilitySystemComponent
end

function Battler:getAttr(name, base)
    local abilitySystem = self:getAbilitySystemComponent()
    if base then
        return abilitySystem:getNumericAttributeBase(name)
    end
    return abilitySystem:getNumericAttribute(name)
end

function Battler:setAttr(name, value)
    self:getAbilitySystemComponent():setNumericAttributeBase(name, value)
end

function Battler:addAttr(name, delta)
    self:setAttr(name, self:getAttr(name, true) + delta)
end

function Battler:playAttackAnimationAt(scene, targetPosition)
    if not bool(self.attributes.ANIMATION_KEY) then
        return nil
    end
    local animationData = Data.GetAnimation(self.attributes.ANIMATION_KEY)
    local Animation = GlobalCore.Animation
    local animation = Animation.new(animationData, true)
    local halfCell = Engine.GetCellSize() * 0.5
    animation:setPosition(sf.Vector2f.new(targetPosition.x + halfCell, targetPosition.y + halfCell))
    scene:addAnim(animation)
    return animation
end

return class(Battler)
