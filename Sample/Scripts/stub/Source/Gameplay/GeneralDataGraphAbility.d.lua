---@meta Source.Gameplay.GeneralDataGraphAbility

---@class Source.Gameplay.GeneralDataGraphAbility: GlobalCore.GameplayAbility
---@field generalType string
---@field memberID    string
---@field graphEvent  string
---@field new         fun(generalType: string, memberID: string, graphEvent: string, triggerTags?: string[]): Source.Gameplay.GeneralDataGraphAbility
local GeneralDataGraphAbility = {}

---@param generalType  string
---@param memberID     string
---@param graphEvent   string
---@param triggerTags? string[]
function GeneralDataGraphAbility:init(generalType, memberID, graphEvent, triggerTags) end

return GeneralDataGraphAbility
