local GameplayEffectSpec = {}

GameplayEffectSpec.effect = nil
GameplayEffectSpec.eventData = nil
GameplayEffectSpec.stacks = 1
GameplayEffectSpec.sourceKey = nil

function GameplayEffectSpec:init(effect, eventData, stacks, sourceKey)
    self.effect = assert(effect, "Gameplay Effect is required")
    self.eventData = eventData
    self.stacks = stacks or 1
    self.sourceKey = sourceKey
    assert(math.type(self.stacks) == "integer" and self.stacks > 0, "Gameplay Effect stacks must be positive")
end

return class(GameplayEffectSpec)
