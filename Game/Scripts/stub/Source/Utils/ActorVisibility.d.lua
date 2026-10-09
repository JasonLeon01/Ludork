---@meta Source.Utils.ActorVisibility

local ActorVisibility = {}

--- Evaluate declarative Actor visibility without subscribing or changing any game state.
--- An empty variable preserves authored visibility; non-empty conditions replace it.
---@param variable        string
---@param operator        string
---@param comparison      number | boolean | string
---@param authoredVisible boolean
---@param variables       table<string, Source.GameInstance.RecordValue>
---@return boolean
function ActorVisibility.Evaluate(variable, operator, comparison, authoredVisible, variables) end

return ActorVisibility
