local GameplayEffect = {}

GameplayEffect.id = ""
GameplayEffect.durationPolicy = "Instant"
GameplayEffect.stackingPolicy = "None"
GameplayEffect.modifiers = {}
GameplayEffect.grantedTags = {}
GameplayEffect.grantedAbilities = {}
GameplayEffect.data = {}

function GameplayEffect:init(values)
    values = values or {}
    self.id = values.id or Class.type(self).id or ""
    self.durationPolicy = values.durationPolicy or Class.type(self).durationPolicy or "Instant"
    self.stackingPolicy = values.stackingPolicy or Class.type(self).stackingPolicy or "None"
    self.modifiers = deepcopy(values.modifiers or Class.type(self).modifiers or {})
    self.grantedTags = deepcopy(values.grantedTags or Class.type(self).grantedTags or {})
    self.grantedAbilities = copy(values.grantedAbilities or Class.type(self).grantedAbilities or {})
    self.data = deepcopy(values.data or Class.type(self).data or {})
    assert(
        self.durationPolicy == "Instant" or self.durationPolicy == "Infinite",
        "Unsupported Gameplay Effect duration policy: " .. tostring(self.durationPolicy)
    )
    assert(
        self.stackingPolicy == "None" or self.stackingPolicy == "Aggregate",
        "Unsupported Gameplay Effect stacking policy: " .. tostring(self.stackingPolicy)
    )
end

return class(GameplayEffect)
