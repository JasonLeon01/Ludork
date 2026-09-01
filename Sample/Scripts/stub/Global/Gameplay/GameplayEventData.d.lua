---@meta Global.Gameplay.GameplayEventData

---@class Global.Gameplay.GameplayEventData
---@field instigator any
---@field target     any
---@field eventTag   string
---@field payload    table<string, any>
---@field new        fun(values?: { instigator?: any, target?: any, eventTag?: string, payload?: table<string, any> }): Global.Gameplay.GameplayEventData
local GameplayEventData = {}

---@param values? { instigator?: any, target?: any, eventTag?: string, payload?: table<string, any> }
function GameplayEventData:init(values) end

return GameplayEventData
