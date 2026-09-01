local GameplayAbilityResult = require("Global.Gameplay.GameplayAbilityResult")

local GameplayAbility = {}

GameplayAbility.id = ""
GameplayAbility.priority = 0
GameplayAbility.abilityTags = {}
GameplayAbility.requiredTags = {}
GameplayAbility.blockedTags = {}
GameplayAbility.triggerTags = {}

function GameplayAbility:init(values)
    values = values or {}
    self.id = values.id or Class.type(self).id or ""
    self.priority = values.priority or Class.type(self).priority or 0
    self.abilityTags = deepcopy(values.abilityTags or Class.type(self).abilityTags or {})
    self.requiredTags = deepcopy(values.requiredTags or Class.type(self).requiredTags or {})
    self.blockedTags = deepcopy(values.blockedTags or Class.type(self).blockedTags or {})
    self.triggerTags = deepcopy(values.triggerTags or Class.type(self).triggerTags or {})
end

function GameplayAbility:canActivate(abilitySystem, _eventData)
    for _, tag in ipairs(self.requiredTags) do
        if not abilitySystem:hasMatchingGameplayTag(tag) then
            return GameplayAbilityResult.Failure("MissingRequiredTag", { tag = tag })
        end
    end
    for _, tag in ipairs(self.blockedTags) do
        if abilitySystem:hasMatchingGameplayTag(tag) then
            return GameplayAbilityResult.Failure("BlockedByTag", { tag = tag })
        end
    end
    return GameplayAbilityResult.Success()
end

---@diagnostic disable-next-line: unused, base virtual method intentionally ignores its receiver and arguments
function GameplayAbility:calculate(_abilitySystem, _eventData)
    return GameplayAbilityResult.Success()
end

function GameplayAbility:activate(abilitySystem, eventData)
    return self:calculate(abilitySystem, eventData)
end

return class(GameplayAbility)
