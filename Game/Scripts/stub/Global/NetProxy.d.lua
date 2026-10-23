---@meta Global.NetProxy

---@class Global.NetProxy
---@field private _moduleName string
---@field private _params Global.Net.Value
---@field private _snapshot table<Global.Net.Key, Global.Net.Value>
---@field private _lastOperation Engine.AsyncOperation | nil
local NetProxy = {}

--- Subclasses that override the constructor must call NetProxy.init with these arguments.
---@param moduleName string
---@param params Global.Net.Value
---@param snapshot table<Global.Net.Key, Global.Net.Value>
function NetProxy:init(moduleName, params, snapshot) end

--- Read a local snapshot field. Tables are returned as independent copies.
--- Business getters should read their fields through this method.
---@param key Global.Net.Key
---@return Global.Net.Value
function NetProxy:GetValue(key) end

--- Fetch using the original copied parameters, then atomically replace this object's snapshot.
--- Calls are serialized and never coalesced. Pending or failed synchronization preserves old data.
--- On success result.proxy is this same object; on failure it is nil.
---@param callback fun(result: Global.Net.Result<Global.NetProxy>)
---@return Engine.AsyncOperation
function NetProxy:Sync(callback) end

---@cast NetProxy Global.NetProxy & Class.ClassType<Global.NetProxy>
return NetProxy
