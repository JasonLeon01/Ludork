local ActiveGameplayEffect = require("Global.Gameplay.ActiveGameplayEffect")
local AttributeSet = require("Global.Gameplay.AttributeSet")
local GameplayAbility = require("Global.Gameplay.GameplayAbility")
local GameplayAbilityResult = require("Global.Gameplay.GameplayAbilityResult")
local GameplayAbilitySpec = require("Global.Gameplay.GameplayAbilitySpec")
local GameplayEffect = require("Global.Gameplay.GameplayEffect")
local GameplayEffectSpec = require("Global.Gameplay.GameplayEffectSpec")
local GameplayEventData = require("Global.Gameplay.GameplayEventData")

local AbilitySystemComponent = {}

local function tagMatches(tag, query)
    return tag == query or string.startsWith(tag, query .. ".")
end

local function resolveMagnitude(modifier, spec, stacks)
    local magnitude = modifier.magnitude
    if type(magnitude) == "function" then
        magnitude = magnitude(spec, stacks)
    end
    assert(type(magnitude) == "number", "Gameplay Effect modifier magnitude must be numeric")
    return magnitude
end

local function abilitySpecLess(left, right)
    if left.ability.priority ~= right.ability.priority then
        return left.ability.priority > right.ability.priority
    end
    return left.grantOrder < right.grantOrder
end

local function accumulateModifiers(aggregate, name, spec, stacks)
    for _, modifier in ipairs(spec.effect.modifiers) do
        if modifier.attribute == name then
            local magnitude = resolveMagnitude(modifier, spec, stacks)
            if spec.effect.stackingPolicy == "Aggregate" then
                if modifier.operation == "Add" then
                    magnitude = magnitude * stacks
                elseif modifier.operation == "Multiply" then
                    magnitude = magnitude ^ stacks
                end
            end
            if modifier.operation == "Add" then
                aggregate.additive = aggregate.additive + magnitude
            elseif modifier.operation == "Multiply" then
                aggregate.multiplier = aggregate.multiplier * magnitude
            elseif modifier.operation == "Override" then
                aggregate.override = magnitude
            else
                error("Unsupported Gameplay Effect modifier operation: " .. tostring(modifier.operation))
            end
            if modifier.minimum ~= nil then
                if aggregate.minimum == nil then
                    aggregate.minimum = modifier.minimum
                else
                    aggregate.minimum = math.max(aggregate.minimum, modifier.minimum)
                end
            end
        end
    end
end

local function onAttributeWrite(old, new, abilitySystem, name)
    if abilitySystem == nil or abilitySystem._internalAttributeWrite then
        return
    end
    abilitySystem:_validateNumericValue(name, new, "Numeric attribute assignment")
    local baseValues = copy(abilitySystem._baseValues)
    baseValues[name] = new
    abilitySystem:_commitNumericBaseValues(baseValues, { [name] = old })
end

function AbilitySystemComponent:init(owner, attributeSet)
    assert(Class.isInstance(attributeSet, AttributeSet), "Ability System requires an AttributeSet")
    self._owner = owner
    self._attributeSet = attributeSet
    self._baseValues = {}
    self._numericAttributes = {}
    self._abilities = {}
    self._activeEffects = {}
    self._activeEffectOrder = {}
    self._tagCounts = {}
    self._attributeListeners = {}
    self._numericAttributeConstraints = {}
    self._nextAbilityOrder = 1
    self._nextEffectHandle = 1
    self._nextEffectOrder = 1
    self._revision = 0
    self._internalAttributeWrite = false
    self._suppressAttributeListeners = false
    self._attributeMonitorParams = {}
    local attributeType = Class.type(attributeSet)
    for _, name in ipairs(attributeType.ATTRIBUTE_NAMES or {}) do
        local schema = assert(attributeType.SCHEMA[name], "Attribute schema is missing for " .. name)
        if schema.type == "int" or schema.type == "float" then
            self:_validateNumericValue(name, attributeSet[name], "Numeric attribute default")
            self._numericAttributes[#self._numericAttributes + 1] = name
            self._baseValues[name] = attributeSet[name]
            local params = setmetatable({ self, name }, { __mode = "v" })
            self._attributeMonitorParams[#self._attributeMonitorParams + 1] = params
            Class.monitor(attributeSet, name, onAttributeWrite, params, true)
        end
    end
end

function AbilitySystemComponent:_incrementRevision()
    self._revision = self._revision + 1
end

function AbilitySystemComponent:_validateNumericValue(name, value, context)
    local schema = assert(self._attributeSet:getAttributeSchema(name), "Unknown attribute schema: " .. tostring(name))
    assert(type(value) == "number", context .. " must be a number: " .. name)
    assert(value == value and value ~= math.huge and value ~= -math.huge, context .. " must be finite: " .. name)
    if schema.type == "int" then
        assert(math.type(value) == "integer", context .. " must be an integer: " .. name)
    else
        assert(schema.type == "float", "Attribute is not numeric: " .. name)
    end
end

function AbilitySystemComponent:_notifyAttributeChange(name, oldValue, newValue, changeSource)
    if self._suppressAttributeListeners or oldValue == newValue and not changeSource.force then
        return
    end
    for _, listener in ipairs(self._attributeListeners[name] or {}) do
        listener.callback(oldValue, newValue, changeSource, table.unpack(listener.params))
    end
end

function AbilitySystemComponent:_writeCurrent(name, value)
    self:_validateNumericValue(name, value, "Numeric attribute current value")
    local oldValue = self:getNumericAttribute(name)
    if oldValue == value then
        return false
    end
    self._internalAttributeWrite = true
    self._attributeSet[name] = value
    self._internalAttributeWrite = false
    return true
end

function AbilitySystemComponent:_getActiveEffect(handle)
    return self._activeEffects[handle]
end

function AbilitySystemComponent:_allocateEffectHandle()
    self._nextEffectHandle = self._nextEffectHandle + 1
    return self._nextEffectHandle - 1
end

function AbilitySystemComponent:_resolveNumericAttribute(
    name, baseValues, pendingSpec, replacedHandle, replacementStacks, resolvedValues
)
    ---@type { additive: number, multiplier: number, override: number | nil, minimum: number | nil }
    local aggregate = { additive = 0, multiplier = 1, override = nil, minimum = nil }
    for _, handle in ipairs(self._activeEffectOrder) do
        local activeEffect = self:_getActiveEffect(handle)
        if activeEffect ~= nil then
            local stacks = handle == replacedHandle and replacementStacks or activeEffect.stacks
            if stacks ~= nil and stacks > 0 then
                accumulateModifiers(aggregate, name, activeEffect.spec, stacks)
            end
        end
    end
    if pendingSpec ~= nil then
        accumulateModifiers(aggregate, name, pendingSpec, pendingSpec.stacks)
    end
    local current = (baseValues[name] + aggregate.additive) * aggregate.multiplier
    if aggregate.override ~= nil then
        current = aggregate.override
    end
    if aggregate.minimum ~= nil then
        current = math.max(aggregate.minimum, current)
    end
    local constraint = self._numericAttributeConstraints[name]
    if constraint ~= nil then
        current = constraint(current, self, resolvedValues)
    end
    self:_validateNumericValue(name, current, "Numeric attribute current value")
    return current
end

function AbilitySystemComponent:_previewNumericAttributes(baseValues, pendingSpec, replacedHandle, replacementStacks)
    local values = {}
    for _, name in ipairs(self._numericAttributes) do
        if name ~= "HP" then
            values[name] = self:_resolveNumericAttribute(
                name, baseValues, pendingSpec, replacedHandle, replacementStacks, values
            )
        end
    end
    if baseValues.HP ~= nil then
        values.HP = self:_resolveNumericAttribute(
            "HP", baseValues, pendingSpec, replacedHandle, replacementStacks, values
        )
    end
    return values
end

function AbilitySystemComponent:_applyCurrentValues(values, changeContext, oldValueOverrides)
    ---@type boolean
    local changed = false
    local oldValues = {}
    for _, name in ipairs(self._numericAttributes) do
        oldValues[name] = oldValueOverrides ~= nil and oldValueOverrides[name] or self._attributeSet[name]
    end
    self._suppressAttributeListeners = true
    for _, name in ipairs(self._numericAttributes) do
        self:_writeCurrent(name, values[name])
    end
    self._suppressAttributeListeners = false
    for _, name in ipairs(self._numericAttributes) do
        local attributeChange = changeContext
        if changeContext.source == "Base" then
            local oldBase = changeContext.oldBaseValues[name]
            local newBase = changeContext.newBaseValues[name]
            attributeChange = { source = "Base", oldBase = oldBase, newBase = newBase, force = oldBase ~= newBase }
        end
        if oldValues[name] ~= self._attributeSet[name] or attributeChange.force then
            changed = true
        end
        self:_notifyAttributeChange(name, oldValues[name], self._attributeSet[name], attributeChange)
    end
    return changed
end

function AbilitySystemComponent:_commitNumericBaseValues(baseValues, oldValueOverrides)
    local oldBaseValues = self._baseValues
    local currentValues = self:_previewNumericAttributes(baseValues)
    ---@type boolean
    local baseChanged = false
    for _, name in ipairs(self._numericAttributes) do
        if oldBaseValues[name] ~= baseValues[name] then
            baseChanged = true
            break
        end
    end
    self._baseValues = baseValues
    local currentChanged = self:_applyCurrentValues(currentValues, {
        source = "Base",
        force = baseChanged,
        oldBaseValues = oldBaseValues,
        newBaseValues = baseValues
    }, oldValueOverrides
    )
    if baseChanged or currentChanged then
        self:_incrementRevision()
    end
end

---@param tag   string
---@param delta integer
function AbilitySystemComponent:_changeTagCount(tag, delta)
    assert(bool(tag), "Gameplay Tag must not be empty")
    local nextCount = (self._tagCounts[tag] or 0) + delta
    assert(nextCount >= 0, "Gameplay Tag count cannot be negative: " .. tag)
    if nextCount == 0 then
        self._tagCounts[tag] = nil
    else
        self._tagCounts[tag] = nextCount
    end
end

function AbilitySystemComponent:_buildInstantBaseValues(spec)
    local baseValues = copy(self._baseValues)
    ---@type boolean
    local changed = false
    for _, modifier in ipairs(spec.effect.modifiers) do
        local name = modifier.attribute
        local baseValue = baseValues[name]
        assert(baseValue ~= nil, "Unknown Gameplay Effect numeric attribute: " .. tostring(name))
        local magnitude = resolveMagnitude(modifier, spec, spec.stacks)
        if spec.effect.stackingPolicy == "Aggregate" then
            if modifier.operation == "Add" then
                magnitude = magnitude * spec.stacks
            elseif modifier.operation == "Multiply" then
                magnitude = magnitude ^ spec.stacks
            end
        end
        if modifier.operation == "Add" then
            baseValue = baseValue + magnitude
        elseif modifier.operation == "Multiply" then
            baseValue = baseValue * magnitude
        elseif modifier.operation == "Override" then
            baseValue = magnitude
        else
            error("Unsupported Gameplay Effect modifier operation: " .. tostring(modifier.operation))
        end
        if modifier.minimum ~= nil then
            baseValue = math.max(modifier.minimum, baseValue)
        end
        self:_validateNumericValue(name, baseValue, "Gameplay Effect result")
        baseValues[name] = baseValue
        changed = true
    end
    return baseValues, changed
end

function AbilitySystemComponent:_applyInstantEffect(spec)
    local baseValues, changed = self:_buildInstantBaseValues(spec)
    if changed then
        self:_commitNumericBaseValues(baseValues)
    end
    return nil
end

function AbilitySystemComponent:validateGameplayEffectSpec(spec)
    assert(Class.isInstance(spec, GameplayEffectSpec), "Ability System requires GameplayEffectSpec")
    self:_validateEffectSpec(spec)
    if spec.effect.durationPolicy == "Instant" then
        local baseValues, changed = self:_buildInstantBaseValues(spec)
        if changed then
            self:_previewNumericAttributes(baseValues)
        end
        return true
    end
    local existing = self:_findStackableEffect(spec)
    if existing == nil then
        self:_previewNumericAttributes(self._baseValues, spec)
    elseif spec.effect.stackingPolicy == "Aggregate" then
        self:_previewNumericAttributes(self._baseValues, nil, existing.handle, existing.stacks + spec.stacks)
    end
    return true
end

function AbilitySystemComponent:_validateEffectSpec(spec)
    assert(Class.isInstance(spec.effect, GameplayEffect), "Gameplay Effect Spec requires a GameplayEffect")
    assert(type(spec.effect.id) == "string", "Gameplay Effect ID must be a string")
    if spec.effect.stackingPolicy == "None" then
        assert(spec.stacks == 1, "Non-stacking Gameplay Effects require exactly one stack")
    elseif spec.effect.durationPolicy == "Infinite" then
        assert(bool(spec.effect.id), "Aggregate Infinite Gameplay Effects require a non-empty ID")
    end
    for _, modifier in ipairs(spec.effect.modifiers) do
        assert(
            self._baseValues[modifier.attribute] ~= nil,
            "Unknown Gameplay Effect numeric attribute: " .. tostring(modifier.attribute)
        )
        assert(
            modifier.operation == "Add" or modifier.operation == "Multiply" or modifier.operation == "Override",
            "Unsupported Gameplay Effect modifier operation: " .. tostring(modifier.operation)
        )
        local magnitude = resolveMagnitude(modifier, spec, spec.stacks)
        assert(
            magnitude == magnitude and magnitude ~= math.huge and magnitude ~= -math.huge,
            "Gameplay Effect modifier magnitude must be finite"
        )
        if modifier.minimum ~= nil then
            assert(type(modifier.minimum) == "number" and modifier.minimum == modifier.minimum
                    and modifier.minimum ~= math.huge and modifier.minimum ~= -math.huge,
                "Gameplay Effect modifier minimum must be finite")
        end
    end
    for _, tag in ipairs(spec.effect.grantedTags) do
        assert(type(tag) == "string" and bool(tag), "Granted Gameplay Tag must be a non-empty string")
    end
    for _, ability in ipairs(spec.effect.grantedAbilities) do
        self:_validateAbility(ability)
    end
    if spec.effect.durationPolicy == "Instant" then
        assert(#spec.effect.grantedTags == 0, "Instant Gameplay Effects cannot grant tags")
        assert(#spec.effect.grantedAbilities == 0, "Instant Gameplay Effects cannot grant abilities")
    end
end

---@diagnostic disable-next-line: unused, instance validation helper intentionally uses colon dispatch
function AbilitySystemComponent:_validateAbility(ability)
    assert(Class.isInstance(ability, GameplayAbility), "Ability System can only grant GameplayAbility instances")
    assert(type(ability.id) == "string" and bool(ability.id), "Gameplay Ability ID must be a non-empty string")
    assert(type(ability.priority) == "number" and ability.priority == ability.priority
            and ability.priority ~= math.huge and ability.priority ~= -math.huge,
        "Gameplay Ability priority must be finite")
    for _, tags in ipairs({ ability.abilityTags, ability.requiredTags, ability.blockedTags, ability.triggerTags }) do
        for _, tag in ipairs(tags) do
            assert(type(tag) == "string" and bool(tag), "Gameplay Ability Tags must be non-empty strings")
        end
    end
end

function AbilitySystemComponent:_findStackableEffect(spec)
    if not bool(spec.effect.id) then
        return nil
    end
    for _, handle in ipairs(self._activeEffectOrder) do
        local activeEffect = self:_getActiveEffect(handle)
        if activeEffect ~= nil and activeEffect.spec.effect.id == spec.effect.id
            and activeEffect.spec.sourceKey == spec.sourceKey then
            return activeEffect
        end
    end
    return nil
end

function AbilitySystemComponent:getOwner()
    return self._owner
end

function AbilitySystemComponent:getAttributeSet()
    return self._attributeSet
end

function AbilitySystemComponent:getNumericAttribute(name)
    assert(self._baseValues[name] ~= nil, "Unknown numeric attribute: " .. tostring(name))
    return self._attributeSet[name]
end

function AbilitySystemComponent:getNumericAttributeBase(name)
    assert(self._baseValues[name] ~= nil, "Unknown numeric attribute: " .. tostring(name))
    return self._baseValues[name]
end

function AbilitySystemComponent:setNumericAttributeBase(name, value)
    assert(self._baseValues[name] ~= nil, "Unknown numeric attribute: " .. tostring(name))
    self:_validateNumericValue(name, value, "Numeric attribute base")
    local baseValues = copy(self._baseValues)
    baseValues[name] = value
    self:_commitNumericBaseValues(baseValues)
end

function AbilitySystemComponent:setNumericAttributeBases(values)
    local baseValues = copy(self._baseValues)
    for name, value in pairs(values) do
        assert(self._baseValues[name] ~= nil, "Unknown numeric attribute: " .. tostring(name))
        self:_validateNumericValue(name, value, "Numeric attribute base")
        baseValues[name] = value
    end
    self:_commitNumericBaseValues(baseValues)
end

function AbilitySystemComponent:getNumericAttributeBases()
    return copy(self._baseValues)
end

function AbilitySystemComponent:addAttributeChangeListener(name, callback, params)
    assert(self._baseValues[name] ~= nil, "Unknown numeric attribute: " .. tostring(name))
    assert(type(callback) == "function", "Attribute change listener must be a function")
    if self._attributeListeners[name] == nil then
        self._attributeListeners[name] = {}
    end
    self._attributeListeners[name][#self._attributeListeners[name] + 1] = { callback = callback, params = params or {} }
end

function AbilitySystemComponent:setNumericAttributeConstraint(name, callback)
    assert(self._baseValues[name] ~= nil, "Unknown numeric attribute: " .. tostring(name))
    assert(callback == nil or type(callback) == "function", "Numeric attribute constraint must be a function or nil")
    self._numericAttributeConstraints[name] = callback
    local currentValues = self:_previewNumericAttributes(self._baseValues)
    if self:_applyCurrentValues(currentValues, { source = "Constraint", force = false }) then
        self:_incrementRevision()
    end
end

function AbilitySystemComponent:giveAbility(ability, sourceKey)
    self:_validateAbility(ability)
    local spec = GameplayAbilitySpec.new(ability, sourceKey, self._nextAbilityOrder)
    self._nextAbilityOrder = self._nextAbilityOrder + 1
    self._abilities[#self._abilities + 1] = spec
    self:_incrementRevision()
    return spec
end

function AbilitySystemComponent:removeAbilitiesBySource(sourceKey)
    local removed = false
    for index = #self._abilities, 1, -1 do
        if self._abilities[index].sourceKey == sourceKey then
            table.remove(self._abilities, index)
            removed = true
        end
    end
    if removed then
        self:_incrementRevision()
    end
end

function AbilitySystemComponent:tryActivateAbility(abilityID, eventData)
    eventData = eventData or GameplayEventData.new()
    local specs = {}
    for _, spec in ipairs(self._abilities) do
        if spec.ability.id == abilityID then
            specs[#specs + 1] = spec
        end
    end
    table.sort(specs, abilitySpecLess)
    if #specs == 0 then
        return GameplayAbilityResult.Failure("AbilityNotFound", { abilityID = abilityID })
    end
    for _, spec in ipairs(specs) do
        local gate = spec.ability:canActivate(self, eventData)
        assert(Class.isInstance(gate, GameplayAbilityResult), "Gameplay Ability gate must return GameplayAbilityResult")
        if gate.ok then
            local result = spec.ability:activate(self, eventData)
            assert(
                Class.isInstance(result, GameplayAbilityResult), "Gameplay Ability must return GameplayAbilityResult"
            )
            return result
        end
    end
    return GameplayAbilityResult.Failure("AbilityNotActivated", { abilityID = abilityID })
end

function AbilitySystemComponent:handleGameplayEvent(eventData)
    assert(Class.isInstance(eventData, GameplayEventData), "Gameplay Event must be GameplayEventData")
    assert(bool(eventData.eventTag), "Gameplay Event Tag must not be empty")
    local specs = {}
    for _, spec in ipairs(self._abilities) do
        for _, triggerTag in ipairs(spec.ability.triggerTags) do
            if tagMatches(eventData.eventTag, triggerTag) then
                specs[#specs + 1] = spec
                break
            end
        end
    end
    table.sort(specs, abilitySpecLess)
    local results = {}
    for _, spec in ipairs(specs) do
        local gate = spec.ability:canActivate(self, eventData)
        assert(Class.isInstance(gate, GameplayAbilityResult), "Gameplay Ability gate must return GameplayAbilityResult")
        if gate.ok then
            local result = spec.ability:activate(self, eventData)
            assert(
                Class.isInstance(result, GameplayAbilityResult), "Gameplay Ability must return GameplayAbilityResult"
            )
            results[#results + 1] = result
        else
            results[#results + 1] = gate
        end
    end
    return results
end

function AbilitySystemComponent:applyGameplayEffectSpec(spec)
    assert(Class.isInstance(spec, GameplayEffectSpec), "Ability System requires GameplayEffectSpec")
    self:_validateEffectSpec(spec)
    if spec.effect.durationPolicy == "Instant" then
        return self:_applyInstantEffect(spec)
    end
    local existing = self:_findStackableEffect(spec)
    if existing ~= nil then
        if spec.effect.stackingPolicy == "Aggregate" then
            local replacementStacks = existing.stacks + spec.stacks
            local currentValues = self:_previewNumericAttributes(
                self._baseValues, nil, existing.handle, replacementStacks
            )
            existing.stacks = replacementStacks
            self:_applyCurrentValues(currentValues, { source = "Effect", force = false })
            self:_incrementRevision()
        end
        return existing.handle
    end
    local currentValues = self:_previewNumericAttributes(self._baseValues, spec)
    local handle = self:_allocateEffectHandle()
    local activeEffect = ActiveGameplayEffect.new(handle, spec, self._nextEffectOrder)
    self._nextEffectOrder = self._nextEffectOrder + 1
    self._activeEffects[handle] = activeEffect
    self._activeEffectOrder[#self._activeEffectOrder + 1] = handle
    for _, tag in ipairs(spec.effect.grantedTags) do
        self:_changeTagCount(tag, 1)
    end
    for _, ability in ipairs(spec.effect.grantedAbilities) do
        activeEffect.grantedAbilitySpecs[#activeEffect.grantedAbilitySpecs + 1] = self:giveAbility(
            ability, activeEffect
        )
    end
    self:_applyCurrentValues(currentValues, { source = "Effect", force = false })
    self:_incrementRevision()
    return handle
end

function AbilitySystemComponent:removeActiveGameplayEffect(handle, stacks)
    local activeEffect = self:_getActiveEffect(handle)
    if activeEffect == nil then
        return false
    end
    local removeStacks = stacks or activeEffect.stacks
    assert(math.type(removeStacks) == "integer" and removeStacks > 0, "Removed stacks must be positive")
    if activeEffect.spec.effect.stackingPolicy == "Aggregate" and removeStacks < activeEffect.stacks then
        local replacementStacks = activeEffect.stacks - removeStacks
        local currentValues = self:_previewNumericAttributes(
            self._baseValues, nil, activeEffect.handle, replacementStacks
        )
        activeEffect.stacks = replacementStacks
        self:_applyCurrentValues(currentValues, { source = "Effect", force = false })
        self:_incrementRevision()
        return true
    end
    local currentValues = self:_previewNumericAttributes(self._baseValues, nil, activeEffect.handle, 0)
    self._activeEffects[handle] = nil
    local orderIndex = table.index(self._activeEffectOrder, handle)
    if orderIndex ~= nil then
        table.remove(self._activeEffectOrder, orderIndex)
    end
    for _, tag in ipairs(activeEffect.spec.effect.grantedTags) do
        self:_changeTagCount(tag, -1)
    end
    self:removeAbilitiesBySource(activeEffect)
    self:_applyCurrentValues(currentValues, { source = "Effect", force = false })
    self:_incrementRevision()
    return true
end

function AbilitySystemComponent:getActiveEffectStacks(effectID)
    local stacks = 0
    for _, handle in ipairs(self._activeEffectOrder) do
        local activeEffect = self:_getActiveEffect(handle)
        if activeEffect ~= nil and activeEffect.spec.effect.id == effectID then
            stacks = stacks + activeEffect.stacks
        end
    end
    return stacks
end

function AbilitySystemComponent:getActiveGameplayEffects()
    local result = {}
    for _, handle in ipairs(self._activeEffectOrder) do
        local activeEffect = self:_getActiveEffect(handle)
        if activeEffect ~= nil then
            result[#result + 1] = activeEffect
        end
    end
    return result
end

function AbilitySystemComponent:hasMatchingGameplayTag(tag)
    for ownedTag, count in pairs(self._tagCounts) do
        if count > 0 and tagMatches(ownedTag, tag) then
            return true
        end
    end
    return false
end

function AbilitySystemComponent:getRevision()
    return self._revision
end

return class(AbilitySystemComponent)
