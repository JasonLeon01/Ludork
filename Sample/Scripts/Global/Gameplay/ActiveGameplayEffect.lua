local ActiveGameplayEffect = {}

ActiveGameplayEffect.handle = 0
ActiveGameplayEffect.spec = nil
ActiveGameplayEffect.stacks = 1
ActiveGameplayEffect.applicationOrder = 0
ActiveGameplayEffect.grantedAbilitySpecs = {}

function ActiveGameplayEffect:init(handle, spec, applicationOrder)
    self.handle = handle
    self.spec = assert(spec, "Gameplay Effect Spec is required")
    self.stacks = spec.stacks
    self.applicationOrder = applicationOrder
    self.grantedAbilitySpecs = {}
end

return class(ActiveGameplayEffect)
