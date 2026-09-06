local GlobalCore = require("GlobalCore")

local GameplayAbility = GlobalCore.GameplayAbility

---@class (partial) Source.Gameplay.SpecialAbilities.PassiveTagAbility
local PassiveTagAbility = {}

---@param specialID string
function PassiveTagAbility:init(specialID)
    GameplayAbility.init(self, {})
    self.id = "Special." .. specialID
end

return class(PassiveTagAbility, GameplayAbility)
