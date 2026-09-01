local GameplayAbilitySpec = {}

GameplayAbilitySpec.ability = nil
GameplayAbilitySpec.sourceKey = nil
GameplayAbilitySpec.grantOrder = 0

function GameplayAbilitySpec:init(ability, sourceKey, grantOrder)
    self.ability = assert(ability, "Gameplay Ability is required")
    self.sourceKey = sourceKey
    self.grantOrder = grantOrder or 0
end

return class(GameplayAbilitySpec)
