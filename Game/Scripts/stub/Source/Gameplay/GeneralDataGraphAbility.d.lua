---@meta Source.Gameplay.GeneralDataGraphAbility

---@class Source.Gameplay.GeneralDataGraphAbility: GlobalCore.GameplayAbility
---@field generalType          string
---@field memberID             string
---@field graphEvent           string
---@field new                  fun(generalType: string, memberID: string, graphEvent: string, triggerTags?: string[]): Source.Gameplay.GeneralDataGraphAbility
---@field private _RESULT_CODE { NO_GRAPH: Source.Gameplay.GeneralDataGraphAbility.ResultCode, EXECUTABLE_GRAPH: Source.Gameplay.GeneralDataGraphAbility.ResultCode, GRAPH_EXECUTED: Source.Gameplay.GeneralDataGraphAbility.ResultCode }
local GeneralDataGraphAbility = {}

---@enum Source.Gameplay.GeneralDataGraphAbility.ResultCode
GeneralDataGraphAbility._RESULT_CODE = {
    NO_GRAPH = "NoGraph",
    EXECUTABLE_GRAPH = "ExecutableGraph",
    GRAPH_EXECUTED = "GraphExecuted"
}

---@param generalType  string
---@param memberID     string
---@param graphEvent   string
---@param triggerTags? string[]
function GeneralDataGraphAbility:init(generalType, memberID, graphEvent, triggerTags) end

return GeneralDataGraphAbility
