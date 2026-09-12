local GlobalCore = require("GlobalCore")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbility = GlobalCore.GameplayAbility

---@class (partial) Source.Gameplay.SpecialAbilities.PassiveTagAbility
local PassiveTagAbility = {}

---@param specialID string
function PassiveTagAbility:init(specialID)
    GameplayAbility.init(self, {})
    self.id = GameplayConstants.SPECIAL_PREFIX .. specialID
end

return class(PassiveTagAbility, GameplayAbility)
