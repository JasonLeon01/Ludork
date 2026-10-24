---@meta Internal.Net.Operations

---@class Internal.Net.Response
---@field ok                boolean
---@field code              string
---@field message           string
---@field data              table<Global.Net.Key, Global.Net.Value> | nil
---@field retryAfterSeconds integer | nil

local Operations = {}

---@param moduleName string
---@param params     Global.Net.Value
---@param transform  fun(snapshot: table<Global.Net.Key, Global.Net.Value>): Global.NetProxy
---@param callback   fun(result: Global.Net.Result<Global.NetProxy>)
---@param after      Engine.AsyncOperation | nil
---@return Engine.AsyncOperation | nil
function Operations.Fetch(moduleName, params, transform, callback, after) end

return Operations
