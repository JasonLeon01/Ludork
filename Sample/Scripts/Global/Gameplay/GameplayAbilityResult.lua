local GameplayAbilityResult = {}

GameplayAbilityResult.ok = false
GameplayAbilityResult.code = ""
GameplayAbilityResult.data = {}

function GameplayAbilityResult:init(ok, code, data)
    self.ok = ok == true
    self.code = code or ""
    self.data = data or {}
end

function GameplayAbilityResult.Success(code, data)
    return GameplayAbilityResult.new(true, code or "Success", data)
end

function GameplayAbilityResult.Failure(code, data)
    assert(bool(code), "Gameplay Ability failure code must not be empty")
    return GameplayAbilityResult.new(false, code, data)
end

return class(GameplayAbilityResult)
